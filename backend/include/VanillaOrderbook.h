#pragma once

#include <map>
#include <unordered_map>
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

class VanillaOrderbook : IOrderbook {
public:
    VanillaOrderbook() = default;
    VanillaOrderbook(const VanillaOrderbook&) = delete;
    void operator=(const VanillaOrderbook&) = delete;
    VanillaOrderbook(VanillaOrderbook&&) = delete;
    void operator=(VanillaOrderbook&&) = delete;
    ~VanillaOrderbook() = default;

    Trades AddOrder(OrderPointer order) override;
    void CancelOrder(OrderId orderId) override;
    Trades ModifyOrder(OrderModify order) override;

    std::size_t Size() const override;
    OrderbookLevelInfos GetOrderInfos() const;

private:
    std::vector<OrderPointer> orders_;

    const OrderPointer getBestAsk() const;
    const OrderPointer getBestBid() const;
    const OrderPointer getWorstAsk() const;
    const OrderPointer getWorstBid() const;
    bool orderExists(OrderId orderId) const;

    void CancelOrders(OrderIds orderIds);
    void CancelOrderInternal(OrderId orderId);

    void OnOrderCancelled(OrderPointer order);
    void OnOrderAdded(OrderPointer order);
    void OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled);
    void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);

    bool CanFullyFill(Side side, Price price, Quantity quantity) const;
    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();
};
