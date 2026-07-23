#include <gtest/gtest.h>
#include "tradecore/replay/replay_engine.hpp"

using namespace tradecore::replay;
using namespace tradecore::parser;
using namespace tradecore::matching;
using namespace tradecore::orderbook;

static constexpr std::size_t kMaxOrders = 1000;
static constexpr std::size_t kMaxLevels = 100;

TEST(ReplayEngineTest, SingleStepMode) {
    // Construct fake raw ITCH payloads
    // We will just craft fake BookUpdates and then serialize them manually to bytes?
    // Wait, the ITCHParser::parse expects raw ITCH messages. 
    // We have a parser that reads AddOrder.
    // In Phase 2, `ITCHParser::parse` extracts `BookUpdate` from `std::span<const std::byte>`.
    // Let's create a dummy ITCH Add Order message.
    
    // According to ITCH 5.0, Add Order (No MPID) is 'A', 36 bytes.
    std::vector<std::byte> msg1(36, std::byte{0});
    msg1[0] = std::byte{'A'}; // Type
    // Stock Locate (2), Tracking (2), Timestamp (6), Ref (8), Side (1), Shares (4), Stock (8), Price (4)
    // Ref (Offset 11) = 1
    msg1[18] = std::byte{1}; // LSB of Ref
    // Side (Offset 19) = 'B'
    msg1[19] = std::byte{'B'};
    // Shares (Offset 20) = 100
    msg1[23] = std::byte{100};
    // Price (Offset 32) = 1000
    msg1[35] = std::byte{1000 % 256};
    msg1[34] = std::byte{1000 / 256};
    
    std::vector<std::vector<std::byte>> payloads = {msg1};
    MemoryDataSource source(payloads);
    
    MatchingEngine<kMaxOrders, kMaxLevels> matcher;
    ReplayEngine<kMaxOrders, kMaxLevels, EmptyRiskPipeline> engine(
        source, matcher, PlaybackMode::SingleStep);
        
    EXPECT_EQ(engine.messages_processed(), 0);
    
    bool has_more = engine.step();
    EXPECT_TRUE(has_more);
    EXPECT_EQ(engine.messages_processed(), 1);
    
    // Check if the order made it into the matcher's book
    const auto& book = matcher.book();
    EXPECT_TRUE(book.bids().has_bbo());
    
    has_more = engine.step();
    EXPECT_FALSE(has_more); // EOF
}
