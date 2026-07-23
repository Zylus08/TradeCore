include(../cmake/GoogleBenchmark.cmake)

add_executable(tradecore_benchmarks
    bm_allocator.cpp
    bm_parser.cpp
    bm_orderbook.cpp
    bm_matching.cpp
)

target_link_libraries(tradecore_benchmarks PRIVATE tradecore benchmark::benchmark)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_target(run_contracts
    COMMAND tradecore_benchmarks --benchmark_out_format=json --benchmark_out=benchmark_results.json
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/validate_contracts.py benchmark_results.json
    DEPENDS tradecore_benchmarks
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(NAME PerformanceContracts
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/validate_contracts.py benchmark_results.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
