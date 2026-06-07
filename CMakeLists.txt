cmake_minimum_required(VERSION 3.10)
project(LOB_Engine)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CRITICAL FOR BENCHMARKING: Enable maximum compiler optimization (-O3)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")

# Tell CMake where to find the header files
include_directories(orderBook)

# Group the core engine files
set(LOB_SOURCES
    orderBook/book.cpp
    orderBook/limit.cpp
    orderBook/order.cpp
)

# Build the main network executable 
add_executable(matching_engine network/main.cpp ${LOB_SOURCES})

# --- Benchmarking Configuration ---
# Find the Google Benchmark library
find_package(benchmark REQUIRED)

# Build the benchmark executable
add_executable(benchmark_engine benchmarking/benchmark.cpp ${LOB_SOURCES})

# Link Google Benchmark and threading libraries
target_link_libraries(benchmark_engine benchmark::benchmark pthread)