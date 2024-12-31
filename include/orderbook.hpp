#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <vector>

#include "order.hpp"

/**
 * Priority comparators
 */
struct BuyOrderComparator {
    bool operator()(const Order &a, const Order &b) const;
};

struct SellOrderComparator {
    bool operator()(const Order &a, const Order &b) const;
};

struct Confirmation {
    sockaddr_in clientAddr;
    socklen_t clientAddrLen;
    std::string message;
};

/**
 * OrderBook class encapsulating the logic for:
 * - Storing orders in buy/sell priority queues
 * - Matching orders
 * - Generating confirmations
 * - Measuring performance
 */
class OrderBook {
public:
    OrderBook();
    ~OrderBook() = default;

    // Process a single order (blocking or from a worker thread)
    void processOrder(Order &o);

    // Accessors
    uint64_t ordersProcessed() const { return m_ordersProcessed.load(); }
    uint64_t totalLatencyNs() const { return m_totalLatencyNs.load(); }
    uint64_t minLatencyNs() const { return m_minLatencyNs.load(); }
    uint64_t maxLatencyNs() const { return m_maxLatencyNs.load(); }

    // Generates a single confirmation message
    std::string buildConfirmation(const Order &o, uint64_t filledQuantity, double avgPrice);

private:
    // The two main priority queues
    std::priority_queue<Order, std::vector<Order>, BuyOrderComparator> m_buyOrders;
    std::priority_queue<Order, std::vector<Order>, SellOrderComparator> m_sellOrders;

    // Mutex for concurrency
    std::mutex m_bookMutex;

    // Performance counters
    std::atomic<uint64_t> m_ordersProcessed{0};
    std::atomic<uint64_t> m_totalLatencyNs{0};
    std::atomic<uint64_t> m_minLatencyNs{UINT64_MAX};
    std::atomic<uint64_t> m_maxLatencyNs{0};

    // Core matching logic
    void matchBuyOrder(Order &buyOrder);
    void matchSellOrder(Order &sellOrder);

    // Extended: different advanced order handling
    void handleStopLoss(Order &o);
    bool handleIOC(Order &o);  // immediate-or-cancel
    bool handleFOK(Order &o);  // fill-or-kill
};

#endif // ORDERBOOK_HPP
