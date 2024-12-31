#include "orderbook.hpp"
#include "json_utils.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <mutex>

//////////////////// Priority Comparators ////////////////////
bool BuyOrderComparator::operator()(const Order &a, const Order &b) const {
    if (std::fabs(a.price - b.price) < 1e-12) {
        // Compare timestamps for FIFO
        return a.recvTimestamp > b.recvTimestamp; // older is higher priority
    }
    return a.price < b.price; // higher price = higher priority
}

bool SellOrderComparator::operator()(const Order &a, const Order &b) const {
    if (std::fabs(a.price - b.price) < 1e-12) {
        return a.recvTimestamp > b.recvTimestamp;
    }
    return a.price > b.price; // lower price = higher priority
}

//////////////////// OrderBook ////////////////////
OrderBook::OrderBook() {}

void OrderBook::processOrder(Order &o) {
    auto startProcess = std::chrono::high_resolution_clock::now();

    // CANCEL
    if (o.type == "cancel") {
        // Real system would search the book by ID, remove it if found.
        // We'll just confirm "cancelled" for demo.
        o.status = "cancelled";
        // update stats
        auto endProcess = std::chrono::high_resolution_clock::now();
        uint64_t latNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endProcess - o.recvTimestamp).count();
        m_ordersProcessed.fetch_add(1, std::memory_order_relaxed);
        m_totalLatencyNs.fetch_add(latNs, std::memory_order_relaxed);
        // update min/max
        uint64_t currentMin = m_minLatencyNs.load(std::memory_order_relaxed);
        while (latNs < currentMin && !m_minLatencyNs.compare_exchange_weak(currentMin, latNs)) {}
        uint64_t currentMax = m_maxLatencyNs.load(std::memory_order_relaxed);
        while (latNs > currentMax && !m_maxLatencyNs.compare_exchange_weak(currentMax, latNs)) {}

        return; // no further matching
    }

    if (o.type == "stop-loss") {
        handleStopLoss(o);
    }

    // Immediate or Cancel?
    if (o.type == "ioc") {
        bool filledAtAll = handleIOC(o);
        if (!filledAtAll) {
            // ioc that didn't fill -> rejected or zero fill
            o.status = (o.quantity == 0) ? "executed" : "ioc_no_fill";
        }
    }
    // Fill or Kill?
    else if (o.type == "fok") {
        bool canFill = handleFOK(o);
        if (!canFill) {
            // entire order not fillable -> kill
            o.remainingQuantity = o.quantity;
            o.status = "fok_no_fill";
        }
    }
    else if (o.type == "market" || o.type == "limit") {
        std::lock_guard<std::mutex> lock(m_bookMutex);

        if (o.isBuy()) {
            matchBuyOrder(o);
            if (o.remainingQuantity > 0 && o.type == "limit") {
                m_buyOrders.push(o);
            } else if (o.remainingQuantity == 0) {
                o.status = "executed";
            } else {
                // partial fill
                if (o.remainingQuantity < o.quantity) {
                    o.status = "partially_filled";
                }
            }
        } else if (o.isSell()) {
            matchSellOrder(o);
            if (o.remainingQuantity > 0 && o.type == "limit") {
                m_sellOrders.push(o);
            } else if (o.remainingQuantity == 0) {
                o.status = "executed";
            } else {
                if (o.remainingQuantity < o.quantity) {
                    o.status = "partially_filled";
                }
            }
        } else {
            o.status = "rejected";
        }
    }
    else {
        // unknown type
        o.status = "rejected";
    }

    auto endProcess = std::chrono::high_resolution_clock::now();
    uint64_t latNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endProcess - o.recvTimestamp).count();
    m_ordersProcessed.fetch_add(1, std::memory_order_relaxed);
    m_totalLatencyNs.fetch_add(latNs, std::memory_order_relaxed);

    // track min, max
    uint64_t currentMin = m_minLatencyNs.load(std::memory_order_relaxed);
    while (latNs < currentMin && !m_minLatencyNs.compare_exchange_weak(currentMin, latNs)) {}
    uint64_t currentMax = m_maxLatencyNs.load(std::memory_order_relaxed);
    while (latNs > currentMax && !m_maxLatencyNs.compare_exchange_weak(currentMax, latNs)) {}
}

void OrderBook::matchBuyOrder(Order &buyOrder) {
    while (buyOrder.remainingQuantity > 0 && !m_sellOrders.empty()) {
        Order topSell = m_sellOrders.top();

        double effectiveBuyPrice = (buyOrder.type == "market") ? 1e15 : buyOrder.price;
        if (effectiveBuyPrice < topSell.price) {
            // no more matching
            break;
        }

        m_sellOrders.pop();
        uint64_t tradedQty = std::min(buyOrder.remainingQuantity, topSell.remainingQuantity);
        double tradePrice = topSell.price;

        buyOrder.remainingQuantity -= tradedQty;
        topSell.remainingQuantity -= tradedQty;

        if (topSell.remainingQuantity == 0) {
            topSell.status = "executed";
        } else {
            topSell.status = "partially_filled";
        }

        // If there's still quantity in topSell, push it back
        if (topSell.remainingQuantity > 0) {
            m_sellOrders.push(topSell);
        }
    }
}

void OrderBook::matchSellOrder(Order &sellOrder) {
    while (sellOrder.remainingQuantity > 0 && !m_buyOrders.empty()) {
        Order topBuy = m_buyOrders.top();
        double effectiveSellPrice = (sellOrder.type == "market") ? 0.0 : sellOrder.price;

        if (topBuy.price < effectiveSellPrice) {
            break;
        }

        m_buyOrders.pop();
        uint64_t tradedQty = std::min(sellOrder.remainingQuantity, topBuy.remainingQuantity);
        double tradePrice = topBuy.price;

        sellOrder.remainingQuantity -= tradedQty;
        topBuy.remainingQuantity -= tradedQty;

        if (topBuy.remainingQuantity == 0) {
            topBuy.status = "executed";
        } else {
            topBuy.status = "partially_filled";
        }

        if (topBuy.remainingQuantity > 0) {
            m_buyOrders.push(topBuy);
        }
    }
}

std::string OrderBook::buildConfirmation(const Order &o, uint64_t filledQuantity, double avgPrice) {
    // Build JSON
    std::map<std::string, std::string> fields;
    fields["order_id"] = std::to_string(o.orderId);
    fields["status"] = o.status;
    fields["filled_quantity"] = std::to_string(filledQuantity);
    fields["remaining_quantity"] = std::to_string(o.remainingQuantity);
    fields["average_price"] = std::to_string(avgPrice);

    return buildJsonString(fields);
}

//////////////////// Extended Logic ////////////////////

void OrderBook::handleStopLoss(Order &o) {
    // Very naive approach:
    if (o.isBuy()) {
        double bestSell = 1e15;
        {
            std::lock_guard<std::mutex> lock(m_bookMutex);
            if (!m_sellOrders.empty()) {
                bestSell = m_sellOrders.top().price;
            }
        }
        if (bestSell <= o.stopPrice) {
            // triggers
            o.type = "market";
        } else {
            // store as limit at stopPrice
            o.type = "limit";
            o.price = o.stopPrice;
        }
    } else {
        double bestBuy = 0.0;
        {
            std::lock_guard<std::mutex> lock(m_bookMutex);
            if (!m_buyOrders.empty()) {
                bestBuy = m_buyOrders.top().price;
            }
        }
        if (bestBuy >= o.stopPrice) {
            o.type = "market";
        } else {
            o.type = "limit";
            o.price = o.stopPrice;
        }
    }
}

bool OrderBook::handleIOC(Order &o) {
    // "Immediate Or Cancel": Attempt to match; leftover is canceled
    // We'll do a quick partial match, but no insertion.
    std::lock_guard<std::mutex> lock(m_bookMutex);

    uint64_t originalQty = o.remainingQuantity;
    if (o.isBuy()) {
        matchBuyOrder(o);
    } else if (o.isSell()) {
        matchSellOrder(o);
    }
    // leftover is canceled
    o.remainingQuantity = 0; // effectively canceled leftover
    return (o.remainingQuantity < originalQty);
}

bool OrderBook::handleFOK(Order &o) {
    // "Fill Or Kill": If the entire quantity cannot be matched immediately, kill the order
    // We must see if there's enough quantity on the opposite side to fill it in total.
    // We'll do a quick simulation, then either fill or kill.
    // For simplicity, let's do a rough approach:
    if (o.isBuy()) {
        // Sum up available sell liquidity at or below price
        std::priority_queue<Order, std::vector<Order>, SellOrderComparator> tempQueue = m_sellOrders;
        uint64_t accumQty = 0;
        std::vector<Order> removed;

        while (!tempQueue.empty() && accumQty < o.remainingQuantity) {
            Order topSell = tempQueue.top();
            tempQueue.pop();
            double effectiveBuyPrice = (o.type == "fok") ? o.price : 1e15;
            if (effectiveBuyPrice < topSell.price) {
                break;
            }
            accumQty += topSell.remainingQuantity;
            removed.push_back(topSell);
        }

        if (accumQty >= o.remainingQuantity) {
            // we can fill
            std::lock_guard<std::mutex> lock(m_bookMutex);
            matchBuyOrder(o);
            return true;
        } else {
            // kill
            return false;
        }
    } else {
        // For sells
        std::priority_queue<Order, std::vector<Order>, BuyOrderComparator> tempQueue = m_buyOrders;
        uint64_t accumQty = 0;
        std::vector<Order> removed;

        while (!tempQueue.empty() && accumQty < o.remainingQuantity) {
            Order topBuy = tempQueue.top();
            tempQueue.pop();
            double effectiveSellPrice = (o.type == "fok") ? o.price : 0.0;
            if (topBuy.price < effectiveSellPrice) {
                break;
            }
            accumQty += topBuy.remainingQuantity;
            removed.push_back(topBuy);
        }

        if (accumQty >= o.remainingQuantity) {
            std::lock_guard<std::mutex> lock(m_bookMutex);
            matchSellOrder(o);
            return true;
        } else {
            return false;
        }
    }
}
