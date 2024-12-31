#include <gtest/gtest.h>
#include "orderbook.hpp"

// Test the comparators
TEST(OrderBookTest, BuyOrderComparator) {
    BuyOrderComparator comp;
    Order a(1, "limit", "buy", 50.0, 100);
    Order b(2, "limit", "buy", 60.0, 200);

    // b has higher price, so b < a in the priority queue sense
    EXPECT_TRUE(comp(a, b));  // means "a < b", i.e. a has lower priority
    EXPECT_FALSE(comp(b, a));
}

TEST(OrderBookTest, SellOrderComparator) {
    SellOrderComparator comp;
    Order a(1, "limit", "sell", 50.0, 100);
    Order b(2, "limit", "sell", 60.0, 200);

    // a has lower price, so a < b in the priority queue sense
    EXPECT_FALSE(comp(a, b)); // means "a" has higher priority
    EXPECT_TRUE(comp(b, a));
}

TEST(OrderBookTest, BasicProcessing) {
    OrderBook ob;
    Order o(100, "limit", "buy", 50.0, 100);
    o.recvTimestamp = std::chrono::high_resolution_clock::now();
    ob.processOrder(o);
    // The order is partially unfilled and remains in buy queue
    // We can't directly check the internal queue from here, but
    // we can ensure no crash or error
    EXPECT_GE(ob.ordersProcessed(), (uint64_t)1);
}

TEST(OrderBookTest, CancelOrder) {
    OrderBook ob;
    Order c(200, "cancel", "buy", 0.0, 0);
    auto start = std::chrono::high_resolution_clock::now();
    c.recvTimestamp = start;
    ob.processOrder(c);
    // Should be processed with "cancelled" status
    EXPECT_EQ(ob.ordersProcessed(), (uint64_t)1);
}
