#pragma once

#include <cstdint>
#include <span>
#include <cstddef>

namespace tradecore::parser {

// Envelope isolating transport metadata from the payload
struct alignas(64) MessageEnvelope {
    uint64_t hardware_rx_timestamp_ns; // Provided by NIC/Kernel
    uint64_t sequence_number;          // From MoldUDP64 or PCAP
    uint16_t session_id;               // TCP/UDP flow identifier
    uint16_t payload_length;
    uint32_t _pad;
    
    // Non-owning view of the raw packet bytes residing in the NIC/Arena ring
    std::span<const std::byte> payload;
};

} // namespace tradecore::parser
