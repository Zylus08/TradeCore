#pragma once

#include <concepts>
#include <span>
#include <cstddef>
#include <cstdint>

namespace tradecore::network {

// C++23 Concept for any Transport Layer Backend
// Supports DPDK, AF_XDP, OpenOnload, standard Sockets, PCAP replays, MMAP
template <typename T>
concept PacketReceiver = requires(T receiver) {
    // Polls the interface. Invokes the callback for each received packet.
    // Callback takes a span of bytes (zero-copy).
    // Returns the number of packets processed in this burst.
    { receiver.poll([](std::span<const std::byte> packet) {}) } -> std::same_as<uint32_t>;
    
    // Optional: Returns hardware RX timestamp if supported
    { receiver.last_rx_timestamp_ns() } -> std::same_as<uint64_t>;
};

} // namespace tradecore::network
