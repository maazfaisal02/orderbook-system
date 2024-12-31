# GoogleTest usually must be installed via package manager or from source.
# We'll assume it's available on the system.
find_package(GTest REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

# Create a single test executable for all test files
add_executable(orderbook_tests
    test_main.cpp
    test_order.cpp
    test_orderbook.cpp
    test_integration.cpp
)

target_link_libraries(orderbook_tests
    PRIVATE
    GTest::GTest
    GTest::Main
    orderbook
    threadsafequeue
    jsonutils
    pthread
)

add_test(NAME AllTests COMMAND orderbook_tests)
