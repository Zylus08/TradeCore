#pragma once

#include "tradecore/matching/execution_result.hpp"
#include "tradecore/matching/order_action.hpp"
#include "tradecore/matching/risk_pipeline.hpp"
#include "tradecore/matching/liquidity_analyzer.hpp"
#include "tradecore/orderbook/order_book.hpp"
#include "tradecore/common/assert.hpp"
#include <chrono>

namespace tradecore::matching {

// A macro for verifying local match invariants
#ifndef NDEBUG
#define TRADECORE_VERIFY_MATCH(trade, incoming, resting) \
    do { \
        TRADECORE_ASSERT((trade).quantity <= (incoming).qty); \
        TRADECORE_ASSERT((trade).quantity <= (resting)->remaining_qty); \
        TRADECORE_ASSERT((trade).price == (resting)->price); \
        TRADECORE_ASSERT((trade).aggressor_order_id == (incoming).order_id); \
        TRADECORE_ASSERT((trade).resting_order_id == (resting)->order_id); \
    } while (0)

#define TRADECORE_VERIFY_EXECUTION(result, initial_qty, incoming, book) \
    do { \
        uint32_t total_filled = 0; \
        for (const auto& t : (result).trades) total_filled += t.quantity; \
        TRADECORE_ASSERT(total_filled + (incoming).qty == (initial_qty)); \
        if ((incoming).qty > 0 && ((incoming).type == OrderType::Limit)) { \
            TRADECORE_ASSERT((book).bids().has_bbo() || (book).asks().has_bbo()); \
        } \
    } while (0)
#else
#define TRADECORE_VERIFY_MATCH(trade, incoming, resting) do {} while (0)
#define TRADECORE_VERIFY_EXECUTION(result, initial_qty, incoming, book) do {} while (0)
#endif

template <std::size_t MaxOrders, std::size_t MaxLevels, typename RiskPipelineType = EmptyRiskPipeline>
class MatchingEngine {
public:
    explicit MatchingEngine(RiskPipelineType risk_pipeline = RiskPipelineType{}) 
        : risk_pipeline_(std::move(risk_pipeline)), next_trade_id_(1) {}

    [[nodiscard]] ExecutionResult match(const OrderAction& action) noexcept {
        ExecutionResult result{};

        // 1. Risk Precheck
        auto risk_result = risk_pipeline_.evaluate(action);
        if (!risk_result.ok) {
            result.status = ExecutionStatus::RejectedRisk;
            return result;
        }

        OrderAction remaining = action;
        const uint32_t initial_qty = remaining.qty;

        // 2. FOK Pre-scan
        if (remaining.type == OrderType::FOK) {
            if (!LiquidityAnalyzer::has_liquidity(book_, remaining.side, remaining.qty)) {
                result.status = ExecutionStatus::RejectedInvalid; // FOK failed scan
                return result;
            }
        }

        // 3. Matching
        match_core(remaining, result, (remaining.type == OrderType::Market));

        // 4. Residual Order -> Book Update
        // Discard residual for IOC, FOK, and Market (Market traditionally discards if book exhausted, or converts to limit. Here we discard if no liquidity).
        if (remaining.qty > 0 && remaining.type == OrderType::Limit) {
            // Add remaining to book
            parser::BookUpdate add_upd{};
            add_upd.type = parser::UpdateType::AddOrder;
            add_upd.side = remaining.side;
            add_upd.price = remaining.price;
            add_upd.qty = remaining.qty;
            add_upd.order_id = remaining.order_id;
            add_upd.timestamp_ns = get_timestamp();
            
            book_.add_order(add_upd);
        }

        // 5. Update overall status based on fills
        if (result.trades.empty()) {
            if (remaining.type == OrderType::IOC || remaining.type == OrderType::FOK) {
                result.status = ExecutionStatus::RejectedInvalid; // Nothing filled
            } else {
                result.status = ExecutionStatus::Accepted;
            }
        } else if (remaining.qty == 0) {
            result.status = ExecutionStatus::Filled;
        } else {
            result.status = ExecutionStatus::PartiallyFilled;
        }

        TRADECORE_VERIFY_EXECUTION(result, initial_qty, remaining, book_);

        return result;
    }

    [[nodiscard]] const orderbook::OrderBook<MaxOrders, MaxLevels>& book() const noexcept {
        return book_;
    }

private:
    void match_core(OrderAction& incoming, ExecutionResult& result, bool is_market) noexcept {
        const auto& opposite_side = (incoming.side == 0) ? book_.asks() : book_.bids();
        
        while (incoming.qty > 0 && opposite_side.has_bbo()) {
            const auto* best_level = opposite_side.bbo_level(book_.level_pool());
            
            // Check limit price constraint if not market order
            if (!is_market) {
                if (incoming.side == 0) { // Buy
                    if (best_level->price > incoming.price) break; // Ask too high
                } else { // Sell
                    if (best_level->price < incoming.price) break; // Bid too low
                }
            }

            // Sweep the level
            uint32_t curr_idx = best_level->head_order_idx;
            while (curr_idx != orderbook::kInvalidPoolIndex && incoming.qty > 0) {
                const auto* resting_order = book_.order_pool().at(curr_idx);
                
                uint32_t fill_qty = std::min(incoming.qty, resting_order->remaining_qty);
                
                Trade t{};
                t.trade_id = next_trade_id_++;
                t.aggressor_order_id = incoming.order_id;
                t.resting_order_id = resting_order->order_id;
                t.price = resting_order->price; // Price improvement belongs to aggressor
                t.quantity = fill_qty;
                t.timestamp_ns = get_timestamp();

                TRADECORE_VERIFY_MATCH(t, incoming, resting_order);

                result.trades.push_back(t);
                incoming.qty -= fill_qty;
                
                // Book mutation
                parser::BookUpdate exec_upd{};
                exec_upd.type = parser::UpdateType::ExecuteOrder;
                exec_upd.order_id = resting_order->order_id;
                exec_upd.qty = fill_qty;
                exec_upd.price = t.price;
                exec_upd.match_id = t.trade_id;

                // Move iterator before mutating book
                curr_idx = resting_order->next_in_level;

                book_.execute_order(exec_upd);
            }
        }
    }

    uint64_t get_timestamp() const noexcept {
        return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    orderbook::OrderBook<MaxOrders, MaxLevels> book_;
    RiskPipelineType risk_pipeline_;
    uint64_t next_trade_id_;
};

} // namespace tradecore::matching
