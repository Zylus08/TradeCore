cmake_minimum_required(VERSION 3.28)
project(TradeCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(TRADECORE_BUILD_TESTS "Build tests" ON)
option(TRADECORE_BUILD_BENCHMARKS "Build benchmarks" ON)
option(TRADECORE_BUILD_EXAMPLES "Build examples" ON)
option(TRADECORE_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(TRADECORE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(TRADECORE_ENABLE_UBSAN "Enable UBSanitizer" OFF)

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

# Library target
add_library(tradecore
    # We will add source files here as they are implemented
)

# Avoid error if no source files yet
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/dummy.cpp")
    file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/src/dummy.cpp" "void dummy() {}\n")
endif()
target_sources(tradecore PRIVATE src/dummy.cpp)

target_include_directories(tradecore PUBLIC include)
target_compile_features(tradecore PUBLIC cxx_std_23)
tradecore_set_warnings(tradecore)
tradecore_enable_sanitizers(tradecore)

# Threading (pthread)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(tradecore PUBLIC Threads::Threads)

# Subdirectories
if(TRADECORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(TRADECORE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
