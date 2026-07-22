#include <gtest/gtest.h>
#include "tradecore/parser/itch_parser.hpp"
#include <vector>
#include <cstring>

using namespace tradecore::parser;
using namespace tradecore::parser::itch;

TEST(ITCHParserTest, ParseAddOrder) {
    AddOrderMsg msg{};
    msg.type = 'A';
    msg.stock_locate = 0;
    msg.tracking_number = 0;
    msg.timestamp = tradecore::host_to_net(123456789ULL);
    msg.order_reference_number = tradecore::host_to_net(999ULL);
    msg.buy_sell_indicator = 'B';
    msg.shares = tradecore::host_to_net(100U);
    std::memcpy(msg.stock, "AAPL    ", 8);
    msg.price = tradecore::host_to_net(1500000U);

    std::vector<std::byte> buffer(sizeof(AddOrderMsg));
    std::memcpy(buffer.data(), &msg, sizeof(AddOrderMsg));

    alignas(64) std::byte arena_buf[1024];
    tradecore::memory::ArenaAllocator arena(sizeof(arena_buf), arena_buf);
    TimestampSource time_src;
    SymbolTable sym_tab;
    Statistics stats;
    ParserContext ctx{arena, time_src, sym_tab, stats};

    ITCHParser parser;
    auto result = parser.parse(ctx, buffer);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->type, UpdateType::AddOrder);
    EXPECT_EQ(result->order_id, 999ULL);
    EXPECT_EQ(result->qty, 100U);
    EXPECT_EQ(result->price, 1500000U);
    EXPECT_EQ(result->side, 0); // Buy
}

TEST(ITCHParserTest, ParseUnhandled) {
    std::vector<std::byte> buffer = { std::byte{'Z'}, std::byte{0x00} }; // Unknown type 'Z'
    
    alignas(64) std::byte arena_buf[1024];
    tradecore::memory::ArenaAllocator arena(sizeof(arena_buf), arena_buf);
    TimestampSource time_src;
    SymbolTable sym_tab;
    Statistics stats;
    ParserContext ctx{arena, time_src, sym_tab, stats};

    ITCHParser parser;
    auto result = parser.parse(ctx, buffer);
    
    EXPECT_FALSE(result.has_value());
}
