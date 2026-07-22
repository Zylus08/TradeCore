#include <gtest/gtest.h>
#include "tradecore/memory/arena_allocator.hpp"

using namespace tradecore::memory;

TEST(ArenaAllocatorTest, BasicAllocation) {
    alignas(64) std::byte buffer[1024];
    ArenaAllocator arena(sizeof(buffer), buffer);

    auto* ptr1 = arena.allocate<uint64_t>();
    ASSERT_NE(ptr1, nullptr);
    *ptr1 = 42;

    auto* ptr2 = arena.allocate<uint64_t>();
    ASSERT_NE(ptr2, nullptr);
    *ptr2 = 84;

    EXPECT_EQ(*ptr1, 42);
    EXPECT_EQ(*ptr2, 84);
}

TEST(ArenaAllocatorTest, Alignment) {
    alignas(64) std::byte buffer[1024];
    ArenaAllocator arena(sizeof(buffer), buffer);

    // Allocate 1 byte
    arena.allocate(1, 1);

    // Allocate 8 bytes, requiring 8-byte alignment
    auto* ptr = arena.allocate(8, 8);
    ASSERT_NE(ptr, nullptr);

    auto ptr_val = reinterpret_cast<std::uintptr_t>(ptr);
    EXPECT_EQ(ptr_val % 8, 0);
}

TEST(ArenaAllocatorTest, OutOfMemory) {
    alignas(64) std::byte buffer[16];
    ArenaAllocator arena(sizeof(buffer), buffer);

    auto* ptr1 = arena.allocate(8, 8);
    ASSERT_NE(ptr1, nullptr);

    auto* ptr2 = arena.allocate(16, 8);
    EXPECT_EQ(ptr2, nullptr); // Not enough space
}

TEST(ArenaAllocatorTest, Reset) {
    alignas(64) std::byte buffer[1024];
    ArenaAllocator arena(sizeof(buffer), buffer);

    arena.allocate(512, 8);
    EXPECT_EQ(arena.used_bytes(), 512);

    arena.reset();
    EXPECT_EQ(arena.used_bytes(), 0);

    auto* ptr = arena.allocate(1024, 8);
    ASSERT_NE(ptr, nullptr); // Should succeed after reset
}
