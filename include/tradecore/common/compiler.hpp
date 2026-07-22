#pragma once

// Branch prediction hints
#if defined(__clang__) || defined(__GNUC__)
#define TRADECORE_LIKELY(x) __builtin_expect(!!(x), 1)
#define TRADECORE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define TRADECORE_LIKELY(x) (x)
#define TRADECORE_UNLIKELY(x) (x)
#endif

// Always inline
#if defined(_MSC_VER)
#define TRADECORE_ALWAYS_INLINE __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#define TRADECORE_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define TRADECORE_ALWAYS_INLINE inline
#endif

// No inline
#if defined(_MSC_VER)
#define TRADECORE_NOINLINE __declspec(noinline)
#elif defined(__clang__) || defined(__GNUC__)
#define TRADECORE_NOINLINE __attribute__((noinline))
#else
#define TRADECORE_NOINLINE
#endif
