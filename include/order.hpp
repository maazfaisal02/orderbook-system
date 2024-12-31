#ifndef ORDER_HPP
#define ORDER_HPP

#include <chrono>
#include <cstdint>
#include <netinet/in.h>
#include <string>

/**
 * Order struct capturing all relevant fields, including
 * partial fill tracking and extended attributes.
 */
struct Order {
    uint64_t orderId;
    std::string type;    // "market", "limit", "cancel", "stop-loss", "ioc", "fok", ...
    std::string action;  // "buy" or "sell"
    double price;
    uint64_t quantity;

    // Additional tracking
    uint64_t remainingQuantity;
    std::string status;
    bool isStopOrder;
    double stopPrice;

    // Timestamps
    std::chrono::time_point<std::chrono::high_resolution_clock> recvTimestamp;

    // For sending confirmations back
    sockaddr_in clientAddr{};
    socklen_t clientAddrLen;

    // Constructors
    Order();
    Order(uint64_t orderId,
          const std::string& type,
          const std::string& action,
          double price,
          uint64_t quantity);

    // Utility
    bool isBuy() const { return action == "buy"; }
    bool isSell() const { return action == "sell"; }
};

#endif // ORDER_HPP
