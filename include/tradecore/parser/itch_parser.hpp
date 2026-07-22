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

inline std::optional<BookUpdate> parse_add_order(ParserContext& ctx, std::span<const std::byte> payload) {
    if (payload.size() < sizeof(AddOrderMsg)) {
        ctx.stats.parse_errors++;
        return std::nullopt;
    }

    const auto* msg = reinterpret_cast<const AddOrderMsg*>(payload.data());
    
    BookUpdate update{};
    update.type = UpdateType::AddOrder;
    update.timestamp_ns = tradecore::net_to_host(msg->timestamp) & 0x0000FFFFFFFFFFFFULL;
    update.order_id = tradecore::net_to_host(msg->order_reference_number);
    update.price = tradecore::net_to_host(msg->price);
    update.qty = tradecore::net_to_host(msg->shares);
    update.side = (msg->buy_sell_indicator == 'B') ? 0 : 1;
    
    std::string_view sym(msg->stock, 8);
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
