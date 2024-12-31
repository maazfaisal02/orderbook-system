#include <arpa/inet.h>  // for inet_pton
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "order.hpp"
#include "orderbook.hpp"
#include "thread_safe_queue.hpp"
#include "json_utils.hpp"

/********************************************************************
 * Global state for the server
 ********************************************************************/
static OrderBook g_orderBook;

// Thread-safe queues
static ThreadSafeQueue<Order> g_orderQueue;
static ThreadSafeQueue<Confirmation> g_confirmationQueue;

// For server control
static std::atomic<bool> g_serverRunning{true};

/********************************************************************
 * Utility: parse an Order from JSON
 ********************************************************************/
static Order parseOrderMessage(const std::string &json, const sockaddr_in &clientAddr) {
    Order o;
    o.recvTimestamp = std::chrono::high_resolution_clock::now();
    o.clientAddr = clientAddr;

    auto fields = parseJsonString(json);

    if (fields.find("order_id") != fields.end()) {
        o.orderId = std::stoull(fields["order_id"]);
    }
    if (fields.find("type") != fields.end()) {
        o.type = fields["type"];
        if (o.type == "stop-loss") {
            o.isStopOrder = true;
        }
    }
    if (fields.find("action") != fields.end()) {
        o.action = fields["action"];
    }
    if (fields.find("quantity") != fields.end()) {
        o.quantity = std::stoull(fields["quantity"]);
        o.remainingQuantity = o.quantity;
    }
    if (fields.find("price") != fields.end()) {
        o.price = std::stod(fields["price"]);
    }
    if (o.isStopOrder && fields.find("stop_price") != fields.end()) {
        o.stopPrice = std::stod(fields["stop_price"]);
    }

    return o;
}

/********************************************************************
 * Worker thread: pops orders from the queue and processes them
 ********************************************************************/
static void serverWorkerThread() {
    while (g_serverRunning.load()) {
        Order o = g_orderQueue.pop();
        g_orderBook.processOrder(o);

        // Build a confirmation. Use naive logic for "filled qty" & "avg price"
        uint64_t filledQty = (o.quantity > o.remainingQuantity)
                             ? (o.quantity - o.remainingQuantity)
                             : 0;
        double avgPrice = (filledQty > 0) ? o.price : 0.0;
        std::string msg = g_orderBook.buildConfirmation(o, filledQty, avgPrice);

        // Enqueue
        Confirmation c;
        c.clientAddr = o.clientAddr;
        c.clientAddrLen = o.clientAddrLen;
        c.message = msg;
        g_confirmationQueue.push(c);
    }
}

/********************************************************************
 * Confirmation sender thread
 ********************************************************************/
static void confirmationSenderThread(int serverSock) {
    while (g_serverRunning.load()) {
        Confirmation c = g_confirmationQueue.pop();
        sendto(serverSock, c.message.c_str(), c.message.size(), 0,
               (struct sockaddr*)&c.clientAddr, c.clientAddrLen);
    }
}

/********************************************************************
 * Throughput logger thread
 ********************************************************************/
static void throughputLoggerThread() {
    auto prevTime = std::chrono::steady_clock::now();
    uint64_t prevCount = 0;

    while (g_serverRunning.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto now = std::chrono::steady_clock::now();

        uint64_t count = g_orderBook.ordersProcessed();
        double elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now - prevTime).count();
        uint64_t delta = count - prevCount;
        double tps = (elapsedSec > 0) ? (delta / elapsedSec) : 0.0;

        uint64_t totalLat = g_orderBook.totalLatencyNs();
        double avgLatUs = 0.0;
        if (count > 0) {
            avgLatUs = (totalLat / 1000.0) / count;
        }
        uint64_t minLat = g_orderBook.minLatencyNs();
        uint64_t maxLat = g_orderBook.maxLatencyNs();

        std::cout << "[Server Throughput] " << tps << " orders/sec, "
                  << "AvgLat=" << avgLatUs << "us "
                  << "MinLat=" << (minLat / 1000.0) << "us "
                  << "MaxLat=" << (maxLat / 1000.0) << "us "
                  << "(processed " << count << " total)\n";

        prevTime = now;
        prevCount = count;
    }
}

/********************************************************************
 * Receiver thread
 ********************************************************************/
static void serverReceiverThread(int serverSock) {
    while (g_serverRunning.load()) {
        char buffer[2048];
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        std::memset(&clientAddr, 0, sizeof(clientAddr));
        std::memset(buffer, 0, sizeof(buffer));

        ssize_t recvLen = recvfrom(serverSock, buffer, sizeof(buffer) - 1, 0,
                                   (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (recvLen > 0) {
            buffer[recvLen] = '\0';
            std::string msg(buffer);
            Order o = parseOrderMessage(msg, clientAddr);
            o.clientAddrLen = clientAddrLen;
            g_orderQueue.push(o);
        }
    }
}

/********************************************************************
 * runServer
 ********************************************************************/
static void runServer(const std::string &ip, int port) {
    // Create socket
    int serverSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        perror("inet_pton");
        close(serverSock);
        exit(EXIT_FAILURE);
    }

    if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind");
        close(serverSock);
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on " << ip << ":" << port << std::endl;

    // Start threads
    std::thread receiver(serverReceiverThread, serverSock);
    const int workerCount = 4;
    std::vector<std::thread> workers;
    for (int i = 0; i < workerCount; i++) {
        workers.emplace_back(serverWorkerThread);
    }
    std::thread confirmer(confirmationSenderThread, serverSock);
    std::thread logger(throughputLoggerThread);

    std::cout << "Press ENTER to stop server..." << std::endl;
    std::cin.get();

    // shutdown
    g_serverRunning.store(false);
    // push dummy to unblock
    g_orderQueue.push(Order());
    g_confirmationQueue.push(Confirmation());

    receiver.join();
    for (auto &w : workers) {
        w.join();
    }
    confirmer.join();
    logger.join();

    close(serverSock);
    std::cout << "Server stopped.\n";
}

/********************************************************************
 * main (server)
 ********************************************************************/
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <IP> <PORT>\n";
        return 1;
    }
    std::string ip = argv[1];
    int port = std::stoi(argv[2]);

    runServer(ip, port);
    return 0;
}
