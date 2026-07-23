#pragma once

#include "tradecore/replay/replay_clock.hpp"
#include "tradecore/parser/itch_parser.hpp"
#include "tradecore/matching/matching_engine.hpp"
#include "tradecore/telemetry/latency_tracker.hpp"
#include "tradecore/telemetry/metrics_registry.hpp"
#include <span>
#include <functional>
#include <vector>

namespace tradecore::replay {

// Interface for providing raw data chunks (e.g. from a PCAP file or memory mapped file)
class DataSource {
public:
    virtual ~DataSource() = default;
    
    // Returns the next raw message payload. Empty span indicates EOF.
    virtual std::span<const std::byte> next_payload() = 0;
};

// Memory-backed data source for testing
class MemoryDataSource : public DataSource {
public:
    explicit MemoryDataSource(const std::vector<std::vector<std::byte>>& payloads)
        : payloads_(payloads), idx_(0) {}

    std::span<const std::byte> next_payload() override {
        if (idx_ >= payloads_.size()) return {};
        return payloads_[idx_++];
    }

private:
    std::vector<std::vector<std::byte>> payloads_;
    std::size_t idx_;
};

template <std::size_t MaxOrders, std::size_t MaxLevels, typename RiskPipelineType>
class ReplayEngine {
public:
    using MatcherType = matching::MatchingEngine<MaxOrders, MaxLevels, RiskPipelineType>;

    explicit ReplayEngine(DataSource& source, 
                          MatcherType& matcher,
                          telemetry::MetricsRegistry* registry = nullptr,
                          PlaybackMode mode = PlaybackMode::MaxThroughput,
                          double acceleration_factor = 1.0)
        : source_(source), 
          matcher_(matcher), 
          registry_(registry),
          clock_(mode, acceleration_factor),
          messages_processed_(0) {}

    // Run until EOF or max_messages
    void run(std::size_t max_messages = std::numeric_limits<std::size_t>::max()) {
        std::span<const std::byte> payload;
        
        while (messages_processed_ < max_messages && !(payload = source_.next_payload()).empty()) {
            
            uint64_t start_e2e = telemetry::get_time_ns();
            parser::BookUpdate update{};
            
            // 1. Parse
            {
                uint64_t start_parse = telemetry::get_time_ns();
                update = parser::ITCHParser::parse(payload);
                if (registry_) registry_->parser_latency.record(telemetry::get_time_ns() - start_parse);
            }
            
            if (update.type == parser::UpdateType::Unknown) {
                continue; // Ignore non-book updates
            }

            // 2. Timing
            clock_.wait_until(update.timestamp_ns);

            // 3. Translate BookUpdate to OrderAction for Matcher (if it's an aggressive order)
            // In a real exchange, ITCH feeds don't trigger matching directly (they reflect matching).
            // But for a full simulation replay where we feed historical data to test the matcher,
            // we treat AddOrder as a limit order injection.
            if (update.type == parser::UpdateType::AddOrder) {
                matching::OrderAction action{};
                action.order_id = update.order_id;
                action.client_id = 0; // Replay has no client ID context
                action.price = update.price;
                action.qty = update.qty;
                action.side = update.side;
                action.type = matching::OrderType::Limit;

                // 4. Match
                matching::ExecutionResult result;
                {
                    uint64_t start_match = telemetry::get_time_ns();
                    result = matcher_.match(action);
                    if (registry_) registry_->matcher_latency.record(telemetry::get_time_ns() - start_match);
                }

                if (registry_) {
                    registry_->orders_processed++;
                    registry_->trades_generated += result.trades.size();
                    if (result.status == matching::ExecutionStatus::RejectedRisk) registry_->risk_rejects++;
                }
            } 
            else {
                auto& mutable_book = const_cast<orderbook::OrderBook<MaxOrders, MaxLevels>&>(matcher_.book());
                mutable_book.dispatch(update);
            }

            if (registry_) {
                registry_->messages_processed++;
                registry_->end_to_end_latency.record(telemetry::get_time_ns() - start_e2e);
            }
            messages_processed_++;
        }
    }

    // Step a single message
    bool step() {
        if (messages_processed_ == 0) {
            // Re-configure clock for single step just in case
            clock_ = ReplayClock(PlaybackMode::SingleStep);
        }
        
        std::span<const std::byte> payload = source_.next_payload();
        if (payload.empty()) return false;
        
        // Single step bypasses wait_until internally anyway
        auto update = parser::ITCHParser::parse(payload);
        if (update.type != parser::UpdateType::Unknown) {
             if (update.type == parser::UpdateType::AddOrder) {
                matching::OrderAction action{};
                action.order_id = update.order_id;
                action.price = update.price;
                action.qty = update.qty;
                action.side = update.side;
                action.type = matching::OrderType::Limit;
                matcher_.match(action);
            } else {
                auto& mutable_book = const_cast<orderbook::OrderBook<MaxOrders, MaxLevels>&>(matcher_.book());
                mutable_book.dispatch(update);
            }
        }
        
        messages_processed_++;
        return true;
    }

    [[nodiscard]] std::size_t messages_processed() const noexcept { return messages_processed_; }

private:
    DataSource& source_;
    MatcherType& matcher_;
    telemetry::MetricsRegistry* registry_;
    ReplayClock clock_;
    std::size_t messages_processed_;
};

} // namespace tradecore::replay
