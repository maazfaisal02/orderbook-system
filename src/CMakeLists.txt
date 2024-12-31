# Create libraries for shared code
add_library(order STATIC order.cpp)
add_library(orderbook STATIC orderbook.cpp)
add_library(threadsafequeue STATIC thread_safe_queue.cpp)
add_library(jsonutils STATIC json_utils.cpp)

target_link_libraries(order PUBLIC jsonutils)
target_link_libraries(orderbook PUBLIC order)
target_link_libraries(threadsafequeue PUBLIC)

# Create the server executable
add_executable(orderbook_server main_server.cpp)
target_link_libraries(orderbook_server
    PRIVATE
    orderbook
    threadsafequeue
    jsonutils
    pthread
)

# Create the client executable
add_executable(orderbook_client main_client.cpp)
target_link_libraries(orderbook_client
    PRIVATE
    orderbook
    threadsafequeue
    jsonutils
    pthread
)
