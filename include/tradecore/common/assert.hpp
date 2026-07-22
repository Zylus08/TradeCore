#pragma once

#include "tradecore/common/compiler.hpp"
#include <cstdlib>
#include <iostream>

namespace tradecore::detail {

[[noreturn]] TRADECORE_NOINLINE inline void assert_fail(const char* expr, const char* file,
                                                        int line) {
    std::cerr << "Assertion failed: " << expr << " at " << file << ":" << line << "\n";
    std::abort();
}

[[noreturn]] TRADECORE_NOINLINE inline void verify_fail(const char* expr, const char* file,
                                                        int line) {
    std::cerr << "Invariant violation: " << expr << " at " << file << ":" << line << "\n";
    std::abort();
}

} // namespace tradecore::detail

#ifndef NDEBUG
#define TRADECORE_ASSERT(cond)                                                                     \
    do {                                                                                           \
        if (TRADECORE_UNLIKELY(!(cond))) {                                                         \
            tradecore::detail::assert_fail(#cond, __FILE__, __LINE__);                             \
        }                                                                                          \
    } while (0)
#else
#define TRADECORE_ASSERT(cond)                                                                     \
    do {                                                                                           \
    } while (0)
#endif

#define TRADECORE_VERIFY(cond)                                                                     \
    do {                                                                                           \
        if (TRADECORE_UNLIKELY(!(cond))) {                                                         \
            tradecore::detail::verify_fail(#cond, __FILE__, __LINE__);                             \
        }                                                                                          \
    } while (0)
