include(../cmake/GoogleTest.cmake)

add_executable(tradecore_tests
    unit/memory/test_arena_allocator.cpp
    unit/memory/test_object_pool.cpp
    unit/memory/test_huge_page.cpp
    unit/lockfree/test_spsc_queue.cpp
    unit/lockfree/test_mpsc_queue.cpp
    unit/network/test_moldudp64.cpp
    unit/parser/test_itch_parser.cpp
    unit/orderbook/test_order_book_verifier.cpp
    unit/orderbook/test_book_snapshot.cpp
    unit/orderbook/test_phase3a.cpp
)

target_link_libraries(tradecore_tests PRIVATE tradecore gtest_main)

include(GoogleTest)
gtest_discover_tests(tradecore_tests)
