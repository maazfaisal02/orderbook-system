#include <gtest/gtest.h>
#include "order.hpp"

// Basic tests for the Order struct
TEST(OrderTest, DefaultConstructor) {
    Order o;
    EXPECT_EQ(o.orderId, 0u);
    EXPECT_EQ(o.type, "");
    EXPECT_EQ(o.action, "");
    EXPECT_DOUBLE_EQ(o.price, 0.0);
    EXPECT_EQ(o.quantity, 0u);
    EXPECT_EQ(o.remainingQuantity, 0u);
    EXPECT_EQ(o.status, "open");
    EXPECT_FALSE(o.isStopOrder);
    EXPECT_DOUBLE_EQ(o.stopPrice, 0.0);
}

TEST(OrderTest, ParamConstructor) {
    Order o(123, "limit", "buy", 45.67, 100);
    EXPECT_EQ(o.orderId, 123u);
    EXPECT_EQ(o.type, "limit");
    EXPECT_EQ(o.action, "buy");
    EXPECT_DOUBLE_EQ(o.price, 45.67);
    EXPECT_EQ(o.quantity, 100u);
    EXPECT_EQ(o.remainingQuantity, 100u);
    EXPECT_EQ(o.status, "open");
    EXPECT_FALSE(o.isStopOrder);
}

TEST(OrderTest, isBuySell) {
    Order buyOrder(1, "limit", "buy", 10.0, 50);
    Order sellOrder(2, "market", "sell", 5.0, 20);
    EXPECT_TRUE(buyOrder.isBuy());
    EXPECT_FALSE(buyOrder.isSell());
    EXPECT_FALSE(sellOrder.isBuy());
    EXPECT_TRUE(sellOrder.isSell());
}
