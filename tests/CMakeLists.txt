include(../cmake/GoogleTest.cmake)

add_executable(tradecore_tests
    unit/memory/test_arena_allocator.cpp
    unit/memory/test_object_pool.cpp
    unit/memory/test_huge_page.cpp
)

target_link_libraries(tradecore_tests PRIVATE tradecore gtest_main)

include(GoogleTest)
gtest_discover_tests(tradecore_tests)
