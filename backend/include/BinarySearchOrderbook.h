#pragma once

#include <map>
#include <unordered_map>
#include <deque>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

#include "Usings.h"
#include "Order.h"
#include "OrderModify.h"
#include "OrderbookLevelInfos.h"
#include "Trade.h"
#include "IOrderbook.h"

class BinarySearchOrderbook : IOrderbook {
public:
    BinarySearchOrderbook() = default;
    BinarySearchOrderbook(const BinarySearchOrderbook&) = delete;
    void operator=(const BinarySearchOrderbook&) = delete;
    BinarySearchOrderbook(BinarySearchOrderbook&&) = delete;
    void operator=(BinarySearchOrderbook&&) = delete;
    ~BinarySearchOrderbook() = default;

    Trades AddOrder(OrderPointer order) override;
    void CancelOrder(OrderId orderId) override;
    Trades ModifyOrder(OrderModify order) override;

    std::size_t Size() const override;
    OrderbookLevelInfos GetOrderInfos() const;

private:
    std::deque<OrderPointer> askOrders_;
    std::deque<OrderPointer> bidOrders_;

    const OrderPointer getBestAsk() const;
    const OrderPointer getBestBid() const;
    const OrderPointer getWorstAsk() const;
    const OrderPointer getWorstBid() const;
    bool orderExists(OrderId orderId) const;

    void CancelOrders(OrderIds orderIds);
    void CancelOrderInternal(OrderId orderId);

    bool CanFullyFill(Side side, Price price, Quantity quantity) const;
    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();
};
