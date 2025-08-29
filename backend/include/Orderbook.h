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
#include "ThreadPool.h"

using BidMap = std::map<Price, OrderPointers, std::greater<Price>>;
using AskMap = std::map<Price, OrderPointers, std::less<Price>>;

class Orderbook {
public:
    Orderbook();
    Orderbook(const Orderbook&) = delete;
    void operator=(const Orderbook&) = delete;
    Orderbook(Orderbook&&) = delete;
    void operator=(Orderbook&&) = delete;
    ~Orderbook();

    struct IOrderbookSnapshotStrategy {
        virtual ~IOrderbookSnapshotStrategy() = default;
        virtual OrderbookLevelInfos Generate(const BidMap& bids, const AskMap& asks) const {
            throw std::logic_error("This snapshot strategy requires a ThreadPool");
        }
        virtual OrderbookLevelInfos Generate(const BidMap& bids, const AskMap& asks, ThreadPool& pool) const {
            return Generate(bids, asks);
        }
    };

    static const IOrderbookSnapshotStrategy& SequentialStrategy();
    static const IOrderbookSnapshotStrategy& AsyncStrategy();
    static const IOrderbookSnapshotStrategy& ThreadPoolStrategy();
    static const IOrderbookSnapshotStrategy& AsyncThreadPoolStrategy();

    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades ModifyOrder(OrderModify order);

    std::size_t Size() const;
    OrderbookLevelInfos GetOrderInfos(const IOrderbookSnapshotStrategy& strategy) const;
    OrderbookLevelInfos GetOrderInfos(const IOrderbookSnapshotStrategy& strategy, ThreadPool& pool) const;

private:
    struct OrderEntry {
        OrderPointer order_{ nullptr };
        OrderPointers::iterator location_;
    };

    struct LevelData {
        Quantity quantity_{};
        Quantity count_{};

        enum class Action {
            Add,
            Remove,
            Match,
        };
    };

    struct SequentialSnapshot : IOrderbookSnapshotStrategy {
        OrderbookLevelInfos Generate(const BidMap& bids, const AskMap& asks) const override;
    };

    struct AsyncSnapshot : IOrderbookSnapshotStrategy {
        OrderbookLevelInfos Generate(const BidMap& bids, const AskMap& asks) const override;
    };

    struct ThreadPoolSnapshot : IOrderbookSnapshotStrategy {
        OrderbookLevelInfos Generate(const BidMap& bids, const AskMap& asks) const override = delete;
        OrderbookLevelInfos Generate(const BidMap& bids, const AskMap& asks, ThreadPool& pool) const override;
    };

    struct AsyncThreadPoolSnapshot : IOrderbookSnapshotStrategy {
        OrderbookLevelInfos Generate(const BidMap& bids, const AskMap& asks, ThreadPool& pool) const override;
    };

    std::unordered_map<Price, LevelData> data_;
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;
    mutable std::mutex ordersMutex_;
    std::thread ordersPruneThread_;
    std::condition_variable shutdownConditionVariable_;
    std::atomic<bool> shutdown_{ false };

    void PruneGoodForDayOrders();

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
