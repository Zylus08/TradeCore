#pragma once

#include "tradecore/parser/book_update.hpp"
#include "tradecore/parser/parser_context.hpp"
#include "tradecore/parser/book_update_validator.hpp"
#include "tradecore/common/endian.hpp"
#include <span>
#include <cstdint>
#include <array>
#include <optional>
#include <cstring>

namespace tradecore::parser {

namespace itch {

// NASDAQ ITCH 5.0 Add Order (No MPID) Message Offsets
constexpr std::size_t kOffsetType = 0;
constexpr std::size_t kOffsetStockLocate = 1;
constexpr std::size_t kOffsetTracking = 5;
constexpr std::size_t kOffsetTimestamp = 7;
constexpr std::size_t kOffsetOrderRef = 11;
constexpr std::size_t kOffsetBuySell = 19;
constexpr std::size_t kOffsetShares = 20;
constexpr std::size_t kOffsetStock = 24;
constexpr std::size_t kOffsetPrice = 32;
constexpr std::size_t kAddOrderLength = 36;

template <typename T>
inline T read_field(std::span<const std::byte> payload, std::size_t offset) noexcept {
    T value;
    // std::memcpy is standard-compliant and avoids strict aliasing violations.
    // Modern compilers completely optimize this into a direct unaligned/aligned 
    // load instruction (e.g., MOV on x86) based on target architecture safety.
    std::memcpy(&value, payload.data() + offset, sizeof(T));
    return tradecore::net_to_host(value);
}

// Special case for 6-byte ITCH timestamp (48-bit)
inline uint64_t read_timestamp(std::span<const std::byte> payload, std::size_t offset) noexcept {
    uint64_t ts = 0;
    // Copy 6 bytes (Big Endian) into the lower 6 bytes of a 64-bit int?
    // Actually, reading 8 bytes and shifting is faster, but we must ensure we don't out-of-bounds read.
    // Since AddOrder is 36 bytes and timestamp is at offset 7, reading 8 bytes (to offset 15) is safe.
    std::memcpy(&ts, payload.data() + offset, 8);
    ts = tradecore::net_to_host(ts);
    // ITCH timestamp is 6 bytes. We read 8 bytes. The top 2 bytes belong to the Tracking Number
    // because we read Big Endian. Wait, if we read 8 bytes starting at offset 7:
    // Byte 7 is TS[0], ..., Byte 12 is TS[5]. Byte 13 is OrderRef[0], Byte 14 is OrderRef[1].
    // Swapping makes it: OrderRef[1], OrderRef[0], TS[5] ... TS[0].
    // So we just mask out the top 16 bits.
    return ts >> 16;
}

inline std::optional<BookUpdate> parse_add_order(ParserContext& ctx, std::span<const std::byte> payload) {
    if (payload.size() < kAddOrderLength) {
        ctx.stats.parse_errors++;
        return std::nullopt;
    }

    BookUpdate update{};
    update.type = UpdateType::AddOrder;
    
    // Explicit, perfectly aligned hardware fetches avoiding packed struct boundaries
    update.timestamp_ns = read_timestamp(payload, kOffsetTimestamp);
    update.order_id = read_field<uint64_t>(payload, kOffsetOrderRef);
    update.price = read_field<uint32_t>(payload, kOffsetPrice);
    update.qty = read_field<uint32_t>(payload, kOffsetShares);
    
    char side_indicator = static_cast<char>(payload[kOffsetBuySell]);
    update.side = (side_indicator == 'B') ? 0 : 1;
    
    std::string_view sym(reinterpret_cast<const char*>(payload.data() + kOffsetStock), 8);
    update.symbol = ctx.symbols.lookup_or_add(sym);

    if (!BookUpdateValidator::validate(update)) {
        ctx.stats.validation_failures++;
        return std::nullopt;
    }

    ctx.stats.messages_parsed++;
    return update;
}

inline std::optional<BookUpdate> parse_ignore(ParserContext&, std::span<const std::byte>) {
    return std::nullopt;
}

} // namespace itch

class ITCHParser {
public:
    using ParserFn = std::optional<BookUpdate> (*)(ParserContext&, std::span<const std::byte>);

    constexpr ITCHParser() : dispatch_table_(build_dispatch_table()) {}

    [[nodiscard]] std::optional<BookUpdate> parse(ParserContext& ctx, std::span<const std::byte> payload) const noexcept {
        if (payload.empty()) {
            ctx.stats.parse_errors++;
            return std::nullopt;
        }

        const auto msg_type = static_cast<uint8_t>(payload[0]);
        return dispatch_table_[msg_type](ctx, payload);
    }

private:
    static constexpr std::array<ParserFn, 256> build_dispatch_table() {
        std::array<ParserFn, 256> table{};
        for (auto& fn : table) {
            fn = &itch::parse_ignore;
        }
        table[static_cast<uint8_t>('A')] = &itch::parse_add_order;
        table[static_cast<uint8_t>('F')] = &itch::parse_add_order;
        return table;
    }

    std::array<ParserFn, 256> dispatch_table_;
};

} // namespace tradecore::parser
