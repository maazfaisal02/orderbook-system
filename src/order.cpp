#include "order.hpp"

Order::Order()
    : orderId(0),
      type(""),
      action(""),
      price(0.0),
      quantity(0),
      remainingQuantity(0),
      status("open"),
      isStopOrder(false),
      stopPrice(0.0),
      clientAddrLen(sizeof(clientAddr)) {
}

Order::Order(uint64_t orderId,
             const std::string& type,
             const std::string& action,
             double price,
             uint64_t quantity)
    : orderId(orderId),
      type(type),
      action(action),
      price(price),
      quantity(quantity),
      remainingQuantity(quantity),
      status("open"),
      isStopOrder(false),
      stopPrice(0.0),
      clientAddrLen(sizeof(clientAddr)) {
}
