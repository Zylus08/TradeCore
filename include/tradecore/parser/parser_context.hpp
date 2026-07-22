#pragma once

#include "tradecore/memory/arena_allocator.hpp"
#include <cstdint>
#include <string_view>

namespace tradecore::parser {

// Stub for now. Maps 8-byte ASCII symbol to a dense 16-bit integer
class SymbolTable {
public:
    [[nodiscard]] uint16_t lookup_or_add(std::string_view /*symbol*/) noexcept {
        return 1; // Dummy return
    }
    [[nodiscard]] bool is_valid(uint16_t /*symbol_index*/) const noexcept {
        return true;
    }
};

class TimestampSource {
public:
    [[nodiscard]] uint64_t now_ns() const noexcept {
        return 0; // Dummy
    }
};

struct Statistics {
    uint64_t messages_parsed{0};
    uint64_t parse_errors{0};
    uint64_t validation_failures{0};
};

struct ParserContext {
    memory::ArenaAllocator& arena;
    TimestampSource& time_source;
    SymbolTable& symbols;
    Statistics& stats;
};

} // namespace tradecore::parser
