include(../cmake/GoogleBenchmark.cmake)

add_executable(tradecore_benchmarks
    bm_allocator.cpp
    bm_parser.cpp
)

target_link_libraries(tradecore_benchmarks PRIVATE tradecore benchmark::benchmark)
