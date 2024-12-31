cmake_minimum_required(VERSION 3.10)
project(OrderBookSystem CXX)

# Enable C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Turn on more warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -O2 -pthread")

# Add an option for building tests
option(BUILD_TESTS "Build the unit tests" ON)

# Include our header files
include_directories(include)

# Subdirectory for source
add_subdirectory(src)

# Subdirectory for tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
