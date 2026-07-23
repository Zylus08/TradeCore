#pragma once

#include "tradecore/orderbook/order_pool.hpp"
#include "tradecore/orderbook/price_level_pool.hpp"
#include "tradecore/orderbook/flat_hash_map.hpp"
#include "tradecore/orderbook/book_side.hpp"
#include "tradecore/orderbook/avl_statistics.hpp"
#include "tradecore/orderbook/order_book_verifier.hpp"
#include "tradecore/parser/book_update.hpp"
#include "tradecore/common/constants.hpp"
#include "tradecore/common/assert.hpp"
#include <span>
#include <array>

namespace tradecore::orderbook {

template <std::size_t MaxOrders, std::size_t MaxLevels>
class OrderBook {
public:
    using MutationFn = void (OrderBook::*)(const parser::BookUpdate&);

    OrderBook() noexcept : order_map_(MaxOrders) {}

    // Main entry point for the dispatcher
    void dispatch(const parser::BookUpdate& update) noexcept {
        uint8_t type_idx = static_cast<uint8_t>(update.type);
        auto handler = kDispatchTable[type_idx];
        if (handler != nullptr) {
            (this->*handler)(update);
        }
    }

    // ========================================================================
    // Market Semantics Mutations
    // ========================================================================

    void add_order(const parser::BookUpdate& update) noexcept {
        // Preconditions
        TRADECORE_ASSERT(update.qty > 0);
        TRADECORE_ASSERT(update.price > 0);
        TRADECORE_ASSERT(!order_map_.contains(update.order_id));

        auto& side = (update.side == 0) ? bids_ : asks_;

        // 1. Find or create level
        uint32_t level_idx = side.find_by_price(level_pool_, update.price);
        PriceLevel* lvl = nullptr;

        if (level_idx == kInvalidPoolIndex) {
            level_idx = side.create_level(update.price, level_pool_);
            TRADECORE_ASSERT(level_idx != kInvalidPoolIndex);
            lvl = level_pool_.at(level_idx);
            side.insert_level(level_idx, level_pool_, stats_);
        } else {
            lvl = level_pool_.at(level_idx);
        }

        // 2. Allocate and initialize Order
        Order* order = order_pool_.allocate();
        TRADECORE_ASSERT(order != nullptr);
        order->order_id      = update.order_id;
        order->price         = update.price;
        order->remaining_qty = update.qty;
        order->timestamp_ns  = update.timestamp_ns;
        order->next_in_level = kInvalidPoolIndex;
        order->side          = update.side;

        uint32_t order_idx = order_pool_.index_of(order);

        // 3. Register in hash map
        bool inserted = order_map_.insert(update.order_id, order_idx);
        TRADECORE_ASSERT(inserted);

        // 4. Update Level FIFO and Aggregates
        if (lvl->tail_order_idx != kInvalidPoolIndex) {
            order_pool_.at(lvl->tail_order_idx)->next_in_level = order_idx;
        } else {
            lvl->head_order_idx = order_idx;
        }
        lvl->tail_order_idx = order_idx;
        
        lvl->total_qty += update.qty;
        lvl->order_count++;

        // Postcondition: Verifier passes
        verify();
    }

    void cancel_order(const parser::BookUpdate& update) noexcept {
        uint32_t order_idx = order_map_.find(update.order_id);
        TRADECORE_ASSERT(order_idx != FlatHashMap::kEmpty);
        
        Order* order = order_pool_.at(order_idx);
        TRADECORE_ASSERT(update.qty <= order->remaining_qty);
        
        order->remaining_qty -= update.qty;
        
        auto& side = (order->side == 0) ? bids_ : asks_;
        uint32_t level_idx = side.find_by_price(level_pool_, order->price);
        TRADECORE_ASSERT(level_idx != kInvalidPoolIndex);
        
        PriceLevel* lvl = level_pool_.at(level_idx);
        lvl->total_qty -= update.qty;

        if (order->remaining_qty == 0) {
            delete_order_internal(order_idx, order, lvl, side, level_idx);
        }
        
        verify();
    }

    void execute_order(const parser::BookUpdate& update) noexcept {
        // Structurally identical to cancel for the orderbook state.
        // Execution stats/accounting would be tracked here.
        cancel_order(update);
    }

    void delete_order(const parser::BookUpdate& update) noexcept {
        uint32_t order_idx = order_map_.find(update.order_id);
        TRADECORE_ASSERT(order_idx != FlatHashMap::kEmpty);
        
        Order* order = order_pool_.at(order_idx);
        auto& side = (order->side == 0) ? bids_ : asks_;
        uint32_t level_idx = side.find_by_price(level_pool_, order->price);
        TRADECORE_ASSERT(level_idx != kInvalidPoolIndex);
        
        PriceLevel* lvl = level_pool_.at(level_idx);
        lvl->total_qty -= order->remaining_qty;
        
        delete_order_internal(order_idx, order, lvl, side, level_idx);
        verify();
    }

    void replace_order(const parser::BookUpdate& update) noexcept {
        // Conceptually: Delete(old_id) + Add(new_id)
        // For replace, BookUpdate usually has order_id as the old one, and maybe match_id as new_id depending on parser.
        // Assuming update.order_id = old, update.match_id = new, update.price = new price, update.qty = new qty.
        parser::BookUpdate del_upd = update;
        delete_order(del_upd);

        parser::BookUpdate add_upd = update;
        add_upd.order_id = update.match_id; // new id
        add_order(add_upd);
        // verify() is called by add_order
    }

    void modify_order(const parser::BookUpdate& update) noexcept {
        uint32_t order_idx = order_map_.find(update.order_id);
        TRADECORE_ASSERT(order_idx != FlatHashMap::kEmpty);
        Order* order = order_pool_.at(order_idx);

        if (update.qty < order->remaining_qty) {
            // Partial cancel (loses no queue priority)
            parser::BookUpdate cancel_upd = update;
            cancel_upd.qty = order->remaining_qty - update.qty;
            cancel_order(cancel_upd);
        } else if (update.qty > order->remaining_qty) {
            // Increase qty (loses queue priority -> replace in-place but at tail)
            // Simplest correct implementation is delete + add.
            uint32_t price = order->price;
            uint8_t side = order->side;
            
            parser::BookUpdate del_upd = update;
            delete_order(del_upd);
            
            parser::BookUpdate add_upd = update;
            add_upd.price = price;
            add_upd.side = side;
            add_order(add_upd);
        }
    }

    void ignore_update(const parser::BookUpdate&) noexcept {}

private:
    void delete_order_internal(uint32_t order_idx, Order* order, PriceLevel* lvl, BookSide<MaxLevels>& side, uint32_t level_idx) noexcept {
        order_map_.erase(order->order_id);
        
        if (lvl->head_order_idx == order_idx) {
            lvl->head_order_idx = order->next_in_level;
            if (lvl->head_order_idx == kInvalidPoolIndex) {
                lvl->tail_order_idx = kInvalidPoolIndex;
            }
        } else {
            uint32_t curr = lvl->head_order_idx;
            while (curr != kInvalidPoolIndex) {
                Order* curr_order = order_pool_.at(curr);
                if (curr_order->next_in_level == order_idx) {
                    curr_order->next_in_level = order->next_in_level;
                    if (order->next_in_level == kInvalidPoolIndex) {
                        lvl->tail_order_idx = curr;
                    }
                    break;
                }
                curr = curr_order->next_in_level;
            }
        }
        
        lvl->order_count--;
        order_pool_.deallocate(order);
        
        if (lvl->order_count == 0) {
            side.remove_and_destroy_level(level_idx, level_pool_, stats_);
        }
    }

    // ========================================================================
    // Verification & Debug
    // ========================================================================

    void verify() const {
#ifndef NDEBUG
        std::span<const PriceLevel> bid_levels(level_pool_.raw_data(), MaxLevels);
        std::span<const PriceLevel> ask_levels(level_pool_.raw_data(), MaxLevels);
        std::span<const Order>      order_pool(order_pool_.raw_data(), MaxOrders);

        auto vr = OrderBookVerifier::verify(
            bid_levels,
            ask_levels,
            order_pool,
            order_map_,
            bids_.avl_root(),
            asks_.avl_root(),
            bids_.bbo_idx(),
            asks_.bbo_idx()
        );
        TRADECORE_ASSERT(vr.ok);
#endif
    }

private:
    OrderPool<MaxOrders>      order_pool_;
    PriceLevelPool<MaxLevels> level_pool_;
    FlatHashMap               order_map_;
    BookSide<MaxLevels>       bids_{0};
    BookSide<MaxLevels>       asks_{1};
    AVLStatistics             stats_;

    // Compile-time dispatcher
    static constexpr std::array<MutationFn, 256> build_dispatch_table() {
        std::array<MutationFn, 256> table{};
        // Default everything to ignore
        for (auto& fn : table) fn = &OrderBook::ignore_update;

        // Map specific ITCH/BookUpdate types
        table[static_cast<uint8_t>(parser::UpdateType::AddOrder)]     = &OrderBook::add_order;
        table[static_cast<uint8_t>(parser::UpdateType::CancelOrder)]  = &OrderBook::cancel_order;
        table[static_cast<uint8_t>(parser::UpdateType::ExecuteOrder)] = &OrderBook::execute_order;
        table[static_cast<uint8_t>(parser::UpdateType::DeleteOrder)]  = &OrderBook::delete_order;
        table[static_cast<uint8_t>(parser::UpdateType::ReplaceOrder)] = &OrderBook::replace_order;
        table[static_cast<uint8_t>(parser::UpdateType::ModifyOrder)]  = &OrderBook::modify_order;
        return table;
    }

    static constexpr auto kDispatchTable = build_dispatch_table();
};

} // namespace tradecore::orderbook
