# Limit Orderbook and Trading Engine

![Limit Orderbook and Trading Engine](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![CMake](https://img.shields.io/badge/CMake-3.10+-brightgreen.svg)
![Tests](https://img.shields.io/badge/tests-passing-brightgreen.svg)

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
  - [Directory Structure](#directory-structure)
  - [Core Components](#core-components)
    - [Order](#order)
    - [OrderBook](#orderbook)
    - [ThreadSafeQueue](#threadsafequeue)
    - [JSON Utilities](#json-utilities)
    - [Client](#client)
    - [Server](#server)
- [Advanced Features](#advanced-features)
- [Performance Optimization](#performance-optimization)
- [Testing](#testing)
  - [Unit Tests](#unit-tests)
  - [Integration Tests](#integration-tests)
- [Build Instructions](#build-instructions)
- [Usage](#usage)
  - [Running the Server](#running-the-server)
  - [Running the Client](#running-the-client)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)

## Overview

The **Limit Orderbook and Trading Engine** is a high-performance, multithreaded Client/Server application implemented in C++17. It facilitates the submission and processing of various order types (market, limit, cancel, stop-loss, immediate-or-cancel, fill-or-kill) over UDP, maintaining an order book that supports high-throughput trading scenarios. Designed with concurrency and scalability in mind, this system leverages modern C++ features and robust testing practices to ensure reliability and efficiency.

## Features

- **UDP-Based Communication**: Lightweight and low-latency client-server interactions.
- **Multithreaded Processing**: Dedicated threads for receiving, processing, sending confirmations, and logging performance metrics.
- **Advanced Order Types**: Support for market, limit, cancel, stop-loss, immediate-or-cancel (IOC), and fill-or-kill (FOK) orders.
- **Partial Fills**: Orders can be partially filled based on available liquidity.
- **Stop-Loss Orders**: Orders that trigger based on specific price conditions.
- **High Throughput and Low Latency**: Optimized data structures and concurrency mechanisms.
- **Comprehensive Testing**: Unit and integration tests using Google Test ensuring high code coverage and reliability.
- **Modular Design**: Clean separation of concerns with a well-structured codebase.

## Architecture

### Directory Structure

```
orderbook-system
├── CMakeLists.txt
├── include
│   ├── json_utils.hpp
│   ├── order.hpp
│   ├── orderbook.hpp
│   ├── thread_safe_queue.hpp
├── src
│   ├── CMakeLists.txt
│   ├── json_utils.cpp
│   ├── main_client.cpp
│   ├── main_server.cpp
│   ├── order.cpp
│   ├── orderbook.cpp
│   ├── thread_safe_queue.cpp
├── tests
│   ├── CMakeLists.txt
│   ├── test_main.cpp
│   ├── test_order.cpp
│   ├── test_orderbook.cpp
│   ├── test_integration.cpp
└── README.md
```

### Core Components

#### Order

- **File**: `include/order.hpp` & `src/order.cpp`
- **Description**: Represents an individual order with all necessary attributes such as order ID, type, action (buy/sell), price, quantity, status, and timestamps.
- **Key Attributes**:
  - `orderId`: Unique identifier for the order.
  - `type`: Type of order (`market`, `limit`, `cancel`, `stop-loss`, `ioc`, `fok`).
  - `action`: `buy` or `sell`.
  - `price`: Price per unit (relevant for limit orders).
  - `quantity`: Total quantity of the order.
  - `remainingQuantity`: Quantity yet to be filled.
  - `status`: Current status (`open`, `executed`, `partially_filled`, `cancelled`, etc.).
  - `isStopOrder`: Indicates if it's a stop-loss order.
  - `stopPrice`: Trigger price for stop-loss orders.

#### OrderBook

- **File**: `include/orderbook.hpp` & `src/orderbook.cpp`
- **Description**: Manages the collection of buy and sell orders, handles order matching logic, and maintains performance metrics.
- **Key Components**:
  - **Priority Queues**:
    - `m_buyOrders`: Max-heap for buy orders, prioritizing higher prices and earlier timestamps.
    - `m_sellOrders`: Min-heap for sell orders, prioritizing lower prices and earlier timestamps.
  - **Concurrency Control**:
    - `m_bookMutex`: Mutex to protect access to the order book.
  - **Performance Metrics**:
    - `m_ordersProcessed`: Total number of processed orders.
    - `m_totalLatencyNs`: Cumulative latency in nanoseconds.
    - `m_minLatencyNs` & `m_maxLatencyNs`: Minimum and maximum latencies observed.
  - **Matching Logic**:
    - `matchBuyOrder()`: Matches incoming buy orders against existing sell orders.
    - `matchSellOrder()`: Matches incoming sell orders against existing buy orders.
    - `handleStopLoss()`: Processes stop-loss orders based on trigger conditions.
    - `handleIOC()` & `handleFOK()`: Handles immediate-or-cancel and fill-or-kill order types.

#### ThreadSafeQueue

- **File**: `include/thread_safe_queue.hpp` & `src/thread_safe_queue.cpp`
- **Description**: A generic thread-safe queue implemented using mutexes and condition variables to facilitate producer-consumer patterns.
- **Usage**: Utilized for managing incoming orders and outgoing confirmations, ensuring safe concurrent access across multiple threads.

#### JSON Utilities

- **File**: `include/json_utils.hpp` & `src/json_utils.cpp`
- **Description**: Provides simple functions to serialize and deserialize JSON-like strings for order and confirmation messages.
- **Functions**:
  - `buildJsonString()`: Constructs a JSON string from a key-value map.
  - `parseJsonString()`: Parses a JSON string into a key-value map.
  - `escapeJsonString()`: Escapes special characters in JSON strings.

#### Client

- **File**: `src/main_client.cpp`
- **Description**: Acts as the client interface, allowing users to submit various types of orders to the server and receive confirmations.
- **Key Functionalities**:
  - **Order Submission**:
    - Send single or multiple random orders for testing.
    - Enter custom orders via an interactive menu.
  - **Confirmation Handling**:
    - Receives and displays confirmation messages from the server asynchronously.
  - **Concurrency**:
    - Utilizes separate threads for sending orders and receiving confirmations to ensure non-blocking operations.

#### Server

- **File**: `src/main_server.cpp`
- **Description**: Handles incoming orders from clients, processes them according to the order book logic, and sends back confirmations.
- **Key Functionalities**:
  - **Order Receiving**:
    - Listens for incoming UDP messages and enqueues them for processing.
  - **Order Processing**:
    - Utilizes a pool of worker threads to process orders concurrently.
    - Matches orders based on type and price-time priority.
  - **Confirmation Sending**:
    - Sends back confirmation messages to clients asynchronously.
  - **Performance Logging**:
    - Logs throughput metrics and latency statistics periodically.

## Advanced Features

- **Immediate-Or-Cancel (IOC)**: Orders that are partially filled immediately and the remaining portion is canceled if not fully filled.
- **Fill-Or-Kill (FOK)**: Orders that must be fully filled immediately; otherwise, the entire order is canceled.
- **Stop-Loss Orders**: Orders that become active only when certain price conditions are met, providing risk management capabilities.
- **Partial Fills**: Allows orders to be partially filled based on available liquidity, enhancing trading flexibility.
- **High Availability Considerations**: While not implemented, the architecture allows for future enhancements like replication and fault tolerance.

## Performance Optimization

- **Efficient Data Structures**: Utilizes priority queues (`std::priority_queue`) for efficient order matching and `std::map` for quick lookups.
- **Multithreading**: Separates concerns by dedicating threads to specific tasks (receiving, processing, sending confirmations, logging).
- **Lock-Free Queues**: Employs thread-safe queues with minimal locking to reduce contention and enhance concurrency.
- **Batch Processing**: Potential for processing orders in batches to further reduce synchronization overhead.
- **Non-Blocking I/O**: Leverages UDP's non-blocking nature for low-latency communication.

## Testing

Comprehensive testing is implemented using **Google Test**, ensuring reliability and correctness across all components.

### Unit Tests

- **Order Tests**: Validate the `Order` struct's constructors and utility functions.
- **OrderBook Tests**: Test the order matching logic, comparators, and basic processing scenarios.
- **ThreadSafeQueue Tests**: Ensure thread-safe operations for queue implementations.

### Integration Tests

- **Simple Match**: Tests basic order matching between buy and sell orders.
- **Market Order Match**: Validates the processing of market orders against existing limit orders.
- **Stop-Loss Trigger**: Checks the activation of stop-loss orders based on price conditions.
- **IOC and FOK Orders**: Ensures immediate-or-cancel and fill-or-kill orders behave as expected under various conditions.

### Running Tests

After building the project, execute the tests using:

```bash
cd orderbook-system/build/tests
./orderbook_tests
```

All tests should pass, indicating high code coverage and reliability.

## Build Instructions

### Prerequisites

- **C++17 Compiler**: GCC 7.0+ or Clang 5.0+.
- **CMake**: Version 3.10 or higher.
- **Google Test**: Installed on your system.

#### Installing Google Test on Ubuntu

```bash
sudo apt-get update
sudo apt-get install libgtest-dev
cd /usr/src/gtest
sudo cmake .
sudo make
sudo cp libg* /usr/lib/
```

### Building the Project

1. **Clone the Repository**

   ```bash
   git clone https://github.com/yourusername/orderbook-system.git
   cd orderbook-system
   ```

2. **Create Build Directory**

   ```bash
   mkdir build && cd build
   ```

3. **Configure the Project with CMake**

   ```bash
   cmake ..
   ```

4. **Build the Project**

   ```bash
   make
   ```

   This will generate the following executables:
   - `orderbook_server`: The server application. (within build/src directory)
   - `orderbook_client`: The client application. (within build/src directory)
   - `orderbook_tests`: The test suite. (within build/tests directory)

## Usage

### Running the Server

Start the server on one terminal by specifying the IP address and port to listen on.

```bash
./orderbook_server 127.0.0.1 55555
```

- **Parameters**:
  - `127.0.0.1`: IP address to bind the server.
  - `55555`: Port number to listen for incoming orders.

- **Behavior**:
  - Listens for incoming UDP messages from clients.
  - Processes orders using a pool of worker threads.
  - Sends confirmations back to clients.
  - Logs throughput and latency metrics every second.
  - Press **ENTER** in the server terminal to gracefully shut down the server.

### Running the Client

Start the client on a second terminal by specifying the server's IP address and port.

```bash
 ./orderbook_client 127.0.0.1 55555
```

- **Parameters**:
  - `127.0.0.1`: IP address of the server.
  - `55555`: Port number on which the server is listening.

- **Interactive Menu**:

  ```
  [Client Menu]
  1) Send a random order
  2) Send multiple random orders (bulk)
  3) Enter a custom order
  4) Quit
  Select:
  ```

- **Options**:
  1. **Send a Random Order**: Submits a single randomly generated order.
  2. **Send Multiple Random Orders**: Prompts for the number of orders to send, useful for stress testing.
  3. **Enter a Custom Order**: Allows manual entry of order details, including type, action, price, quantity, and stop price for stop-loss orders.
  4. **Quit**: Exits the client application.

- **Confirmation Handling**:
  - Receives and displays confirmation messages from the server asynchronously.


## Contributing

Contributions are welcome! Whether it's improving existing features, adding new functionalities, or enhancing the documentation, your input is valuable.


## License

Distributed under the MIT License. See `LICENSE` for more information.


---

*This project showcases advanced C++ skills, multithreaded programming, network communication, and comprehensive testing.*
