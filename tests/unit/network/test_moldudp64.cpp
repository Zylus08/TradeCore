#include <gtest/gtest.h>
#include "tradecore/network/moldudp64.hpp"
#include <vector>
#include <cstring>

using namespace tradecore::network;

TEST(MoldUDP64Test, DecodePackets) {
    std::vector<std::byte> packet;
    
    // Construct header
    MoldUDP64Header header{};
    std::memcpy(header.session, "TEST000000", 10);
    header.sequence_number = tradecore::host_to_net(100ULL);
    header.message_count = tradecore::host_to_net(2U);
    
    packet.resize(sizeof(header));
    std::memcpy(packet.data(), &header, sizeof(header));
    
    // Message 1 (length 4)
    uint16_t msg1_len = tradecore::host_to_net(4U);
    uint8_t msg1_data[4] = {0x01, 0x02, 0x03, 0x04};
    auto old_size = packet.size();
    packet.resize(old_size + 2 + 4);
    std::memcpy(packet.data() + old_size, &msg1_len, 2);
    std::memcpy(packet.data() + old_size + 2, msg1_data, 4);
    
    // Message 2 (length 2)
    uint16_t msg2_len = tradecore::host_to_net(2U);
    uint8_t msg2_data[2] = {0xAA, 0xBB};
    old_size = packet.size();
    packet.resize(old_size + 2 + 2);
    std::memcpy(packet.data() + old_size, &msg2_len, 2);
    std::memcpy(packet.data() + old_size + 2, msg2_data, 2);
    
    MoldUDP64Decoder decoder;
    int msgs_received = 0;
    
    bool success = decoder.decode(packet, [&](uint64_t seq, std::span<const std::byte> payload) {
        if (msgs_received == 0) {
            EXPECT_EQ(seq, 100ULL);
            EXPECT_EQ(payload.size(), 4);
            EXPECT_EQ(payload[0], std::byte{0x01});
        } else if (msgs_received == 1) {
            EXPECT_EQ(seq, 101ULL);
            EXPECT_EQ(payload.size(), 2);
            EXPECT_EQ(payload[0], std::byte{0xAA});
        }
        msgs_received++;
    });
    
    EXPECT_TRUE(success);
    EXPECT_EQ(msgs_received, 2);
    EXPECT_EQ(decoder.expected_sequence(), 102ULL);
}
