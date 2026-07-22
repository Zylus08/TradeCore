#include <gtest/gtest.h>
#include "tradecore/memory/huge_page.hpp"

using namespace tradecore::memory;

TEST(HugePageAllocatorTest, AllocateAndDeallocate) {
    std::size_t size = 1024 * 1024 * 2; // 2MB
    
    void* ptr = nullptr;
    EXPECT_NO_THROW({
        ptr = HugePageAllocator::allocate(size);
    });
    
    ASSERT_NE(ptr, nullptr);
    
    // Test write access
    auto* bytes = static_cast<std::byte*>(ptr);
    bytes[0] = std::byte{42};
    bytes[size - 1] = std::byte{84};
    
    EXPECT_EQ(bytes[0], std::byte{42});
    EXPECT_EQ(bytes[size - 1], std::byte{84});
    
    EXPECT_NO_THROW({
        HugePageAllocator::deallocate(ptr, size);
    });
}
