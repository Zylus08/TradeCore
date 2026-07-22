#pragma once

#include "tradecore/parser/book_update.hpp"
#include "tradecore/common/endian.hpp"
#include <span>
#include <cstdint>
#include <array>
#include <optional>
#include <cstring>

namespace tradecore::parser {

// Forward declare specific message parsers
namespace itch {

#pragma pack(push, 1)
struct AddOrderMsg {
    char type;
    uint32_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;
    uint64_t order_reference_number;
    char buy_sell_indicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
};
#pragma pack(pop)

// Example parser for ITCH 'A' (Add Order)
inline std::optional<BookUpdate> parse_add_order(std::span<const std::byte> payload) {
    if (payload.size() < sizeof(AddOrderMsg)) return std::nullopt;

    const auto* msg = reinterpret_cast<const AddOrderMsg*>(payload.data());
    
    BookUpdate update{};
    update.type = UpdateType::AddOrder;
    update.timestamp_ns = tradecore::net_to_host(msg->timestamp) & 0x0000FFFFFFFFFFFFULL; // 48-bit nanoseconds
    update.order_id = tradecore::net_to_host(msg->order_reference_number);
    update.price = tradecore::net_to_host(msg->price);
    update.qty = tradecore::net_to_host(msg->shares);
    update.side = (msg->buy_sell_indicator == 'B') ? 0 : 1;
    // Fast copy for symbol mapping (left as raw for now)
    std::memcpy(&update.symbol, msg->stock, 2); 

    return update;
}

// Default fallback parser for unhandled message types
inline std::optional<BookUpdate> parse_ignore(std::span<const std::byte>) {
    return std::nullopt;
}

} // namespace itch

class ITCHParser {
public:
    using ParserFn = std::optional<BookUpdate> (*)(std::span<const std::byte>);

    constexpr ITCHParser() : dispatch_table_(build_dispatch_table()) {}

    // Main entry point for a single ITCH message byte span
    [[nodiscard]] std::optional<BookUpdate> parse(std::span<const std::byte> payload) const noexcept {
        if (payload.empty()) return std::nullopt;

        // The first byte of the ITCH message defines its type
        const auto msg_type = static_cast<uint8_t>(payload[0]);

        // O(1) Indirect call to the registered compile-time parser
        return dispatch_table_[msg_type](payload);
    }

private:
    static constexpr std::array<ParserFn, 256> build_dispatch_table() {
        std::array<ParserFn, 256> table{};
        
        // Initialize all to ignore
        for (auto& fn : table) {
            fn = &itch::parse_ignore;
        }

        // Register known parsers here
        table[static_cast<uint8_t>('A')] = &itch::parse_add_order;
        table[static_cast<uint8_t>('F')] = &itch::parse_add_order; // 'F' is Add Order with MPID, similar struct

        return table;
    }

    std::array<ParserFn, 256> dispatch_table_;
};

} // namespace tradecore::parser
