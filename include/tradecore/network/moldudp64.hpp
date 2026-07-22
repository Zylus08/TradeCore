#pragma once

#include "tradecore/common/endian.hpp"
#include <cstdint>
#include <span>
#include <cstddef>
#include <vector>
#include <string_view>
#include <cstring>

namespace tradecore::network {

#pragma pack(push, 1)
struct MoldUDP64Header {
    char session[10];
    uint64_t sequence_number;
    uint16_t message_count;
};
#pragma pack(pop)

static_assert(sizeof(MoldUDP64Header) == 20, "MoldUDP64 Header must be exactly 20 bytes");

class MoldUDP64Decoder {
public:
    // Takes a raw UDP packet span and calls the callback for each contained message
    template <typename Callback>
    bool decode(std::span<const std::byte> packet, Callback&& on_message) {
        if (packet.size() < sizeof(MoldUDP64Header)) {
            return false;
        }

        const auto* header = reinterpret_cast<const MoldUDP64Header*>(packet.data());
        
        // Network to Host conversion
        uint64_t seq_num = net_to_host(header->sequence_number);
        uint16_t msg_count = net_to_host(header->message_count);

        // Advance to message blocks
        std::size_t offset = sizeof(MoldUDP64Header);
        
        for (uint16_t i = 0; i < msg_count; ++i) {
            if (offset + 2 > packet.size()) return false;

            // Read message length
            uint16_t msg_len;
            std::memcpy(&msg_len, packet.data() + offset, 2);
            msg_len = net_to_host(msg_len);
            offset += 2;

            if (offset + msg_len > packet.size()) return false;

            // Extract the message payload
            std::span<const std::byte> payload(packet.data() + offset, msg_len);
            
            // Invoke the parser callback
            on_message(seq_num + i, payload);

            offset += msg_len;
        }
        
        expected_seq_ = seq_num + msg_count;
        return true;
    }

    [[nodiscard]] uint64_t expected_sequence() const noexcept { return expected_seq_; }
    void set_expected_sequence(uint64_t seq) noexcept { expected_seq_ = seq; }

private:
    uint64_t expected_seq_{1};
};

} // namespace tradecore::network
