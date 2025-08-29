#include <numeric>
#include <chrono>
#include <ctime>
#include <future>

#include "Orderbook.h"

// Strategy singletons
const Orderbook::IOrderbookSnapshotStrategy& Orderbook::SequentialStrategy() {
	static SequentialSnapshot instance;
	return instance;
}

const Orderbook::IOrderbookSnapshotStrategy& Orderbook::AsyncStrategy() {
	static AsyncSnapshot instance;
	return instance;
}

const Orderbook::IOrderbookSnapshotStrategy& Orderbook::ThreadPoolStrategy() {
	static ThreadPoolSnapshot instance;
	return instance;
}

const Orderbook::IOrderbookSnapshotStrategy& Orderbook::AsyncThreadPoolStrategy() {
	static AsyncThreadPoolSnapshot instance;
	return instance;
}

/* Cancels GFD orders at the end of a trading day (4PM).
 * Runs in O(N * log(M)). 
 */
void Orderbook::PruneGoodForDayOrders() {
	using namespace std::chrono;
	const auto end = hours(16);

	while (true) {
		// Compute next 4PM.
		const auto now = system_clock::now();
		const auto now_c = system_clock::to_time_t(now);
		std::tm now_parts;
		localtime_s(&now_parts, &now_c);

		if (now_parts.tm_hour >= end.count())
			now_parts.tm_mday += 1;

		now_parts.tm_hour = end.count();
		now_parts.tm_min = 0;
		now_parts.tm_sec = 0;

		auto next = system_clock::from_time_t(mktime(&now_parts));
		auto till = next - now + milliseconds(100);

		{
			std::unique_lock ordersLock{ ordersMutex_ };

			if (shutdown_.load(std::memory_order_acquire) ||
				shutdownConditionVariable_.wait_for(ordersLock, till) == std::cv_status::no_timeout)
				return;
		}

		OrderIds orderIds;

		{
			std::scoped_lock ordersLock{ ordersMutex_ };

			for (const auto& [_, entry] : orders_) {
				const auto& [order, _] = entry;

				if (order->GetOrderType() != OrderType::GoodForDay)
					continue;

				orderIds.push_back(order->GetOrderId());
			}
		}

		CancelOrders(orderIds);
	}
}

/* Cancels all orders with the given order ids.
 * Runs in O(N * log(M)), where:
 * - N = amount of given order ids and
 * - M = number of distinct price levels.
 */
void Orderbook::CancelOrders(OrderIds orderIds)
{
	std::scoped_lock ordersLock{ ordersMutex_ };

	for (const auto& orderId : orderIds)
		CancelOrderInternal(orderId);
}

/* Cancels the order with the given order id.
 * Runs in O(log(M)) where M is the number of distinct price levels.
 */
void Orderbook::CancelOrderInternal(OrderId orderId)
{
	if (!orders_.contains(orderId)) return;

	const auto [order, iterator] = orders_.at(orderId);
	orders_.erase(orderId);

	if (order->GetSide() == Side::Sell) {
		auto price = order->GetPrice();
		auto& orders = asks_.at(price);

		orders.erase(iterator);
		if (orders.empty()) asks_.erase(price);
	} else {
		auto price = order->GetPrice();
		auto& orders = bids_.at(price);

		orders.erase(iterator);
		if (orders.empty()) bids_.erase(price);
	}

	OnOrderCancelled(order);
}

void Orderbook::OnOrderCancelled(OrderPointer order) {
	UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove);
}

void Orderbook::OnOrderAdded(OrderPointer order) {
	UpdateLevelData(order->GetPrice(), order->GetInitialQuantity(), LevelData::Action::Add);
}

void Orderbook::OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled) {
	UpdateLevelData(price, quantity, isFullyFilled ? LevelData::Action::Remove : LevelData::Action::Match);
}

/* Updates level data corresponding to the given price and quantity based on the given action.
 * Runs in amortized O(1).
 */
void Orderbook::UpdateLevelData(Price price, Quantity quantity, LevelData::Action action) {
	auto& data = data_[price];

	data.count_ += action == LevelData::Action::Remove ? -1 : action == LevelData::Action::Add ? 1 : 0;

	if (action == LevelData::Action::Remove || action == LevelData::Action::Match) {
		data.quantity_ -= quantity;
	} else {
		data.quantity_ += quantity;
	}

	if (data.count_ == 0) data_.erase(price);
}

/* Checks if an order with the given side, price, and quantity can be fully filled.
 * Runs in O(N), where N is the amount of price levels. 
 */
bool Orderbook::CanFullyFill(Side side, Price price, Quantity quantity) const
{
	if (!CanMatch(side, price)) return false;

	std::optional<Price> threshold;

	if (side == Side::Buy) {
		const auto [askPrice, _] = *asks_.begin();
		threshold = askPrice;
	} else {
		const auto [bidPrice, _] = *bids_.begin();
		threshold = bidPrice;
	}

	for (const auto& [levelPrice, levelData] : data_) {
		if (threshold.has_value() &&
			(side == Side::Buy && threshold.value() > levelPrice) ||
			(side == Side::Sell && threshold.value() < levelPrice))
			continue;

		if ((side == Side::Buy && levelPrice > price) ||
			(side == Side::Sell && levelPrice < price))
			continue;

		if (quantity <= levelData.quantity_)
			return true;

		quantity -= levelData.quantity_;
	}

	return false;
}

/* Returns true if an order on the given side and price can be matched against the best available opposite order.
 * For a buy order, checks if it can match the best ask.
 * For a sell order, checks if it can match the best bid.
 * Runs in O(1).
 */
bool Orderbook::CanMatch(Side side, Price price) const
{
	if (side == Side::Buy) {
		if (asks_.empty())
			return false;

		const auto& [bestAsk, _] = *asks_.begin();
		return price >= bestAsk;
	} else {
		if (bids_.empty())
			return false;

		const auto& [bestBid, _] = *bids_.begin();
		return price <= bestBid;
	}
}

/* Matches orders in the orderbook.
 * Runs in O(N * log(M)) where N is the total amount of orders and M is the amount of price levels.
 */
Trades Orderbook::MatchOrders()
{
	Trades trades;
	trades.reserve(orders_.size());

	while (true) {
		if (bids_.empty() || asks_.empty())
			break;

		auto& [bidPrice, bids] = *bids_.begin();
		auto& [askPrice, asks] = *asks_.begin();

		if (bidPrice < askPrice)
			break;

		while (bids.size() && asks.size()) {
			auto bid = bids.front();
			auto ask = asks.front();

			Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

			bid->Fill(quantity);
			ask->Fill(quantity);

			if (bid->IsFilled()) {
				bids.pop_front();
				orders_.erase(bid->GetOrderId());
			}

			if (ask->IsFilled()) {
				asks.pop_front();
				orders_.erase(ask->GetOrderId());
			}

			trades.push_back(Trade{
				TradeInfo{ bid->GetOrderId(), bid->GetPrice(), quantity },
				TradeInfo{ ask->GetOrderId(), ask->GetPrice(), quantity }
			});

			OnOrderMatched(bid->GetPrice(), quantity, bid->IsFilled());
			OnOrderMatched(ask->GetPrice(), quantity, ask->IsFilled());
		}

		if (bids.empty()) {
			bids_.erase(bidPrice);
			data_.erase(bidPrice);
		}

		if (asks.empty()) {
			asks_.erase(askPrice);
			data_.erase(askPrice);
		}
	}

	if (!bids_.empty()) {
		auto& [_, bids] = *bids_.begin();
		auto& order = bids.front();
		if (order->GetOrderType() == OrderType::FillAndKill)
			CancelOrder(order->GetOrderId());
	}

	if (!asks_.empty()) {
		auto& [_, asks] = *asks_.begin();
		auto& order = asks.front();
		if (order->GetOrderType() == OrderType::FillAndKill)
			CancelOrder(order->GetOrderId());
	}

	return trades;
}

Orderbook::Orderbook() : ordersPruneThread_{ [this] { PruneGoodForDayOrders(); } } {}

Orderbook::~Orderbook() {
	shutdown_.store(true, std::memory_order_release);
	shutdownConditionVariable_.notify_one();
	ordersPruneThread_.join();
}

/* Adds an order to the orderbook.
 * Runs in O(N * log(M)) where N is the total amount of orders and M is the amount of price levels.
 */
Trades Orderbook::AddOrder(OrderPointer order) {
	std::scoped_lock ordersLock{ ordersMutex_ };

	if (orders_.contains(order->GetOrderId()))
		return {};

	if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
		return {};

	if (order->GetOrderType() == OrderType::Market) {
		if (order->GetSide() == Side::Buy && !asks_.empty()) {
			const auto& [worstAsk, _] = *asks_.rbegin();
			order->ToGoodTillCancel(worstAsk);
		} else if (order->GetSide() == Side::Sell && !bids_.empty()) {
			const auto& [worstBid, _] = *bids_.rbegin();
			order->ToGoodTillCancel(worstBid);
		} else
			return {};
	}

	if (order->GetOrderType() == OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetInitialQuantity()))
		return {};

	OrderPointers::iterator iterator;

	if (order->GetSide() == Side::Buy) {
		auto& orders = bids_[order->GetPrice()];
		orders.push_back(order);
		iterator = std::prev(orders.end());
	} else {
		auto& orders = asks_[order->GetPrice()];
		orders.push_back(order);
		iterator = std::prev(orders.end());
	}

	orders_.insert({ order->GetOrderId(), OrderEntry{ order, iterator } });

	OnOrderAdded(order);

	return MatchOrders();
}

/* Acquires a lock on the orders and then cancels the order with the given order id.
 * Runs in O(log(M)) where M is the number of distinct price levels.
 */
void Orderbook::CancelOrder(OrderId orderId) {
	std::scoped_lock ordersLock{ ordersMutex_ };

	CancelOrderInternal(orderId);
}

/* Modifies the order with the given order id by first cancelling the order, and then adding a new order with the modified data.
 * Runs in O(N * log(M)) where N is the total amount of orders and M is the amount of price levels.
 */
Trades Orderbook::ModifyOrder(OrderModify order) {
	OrderType orderType;

	{
		std::scoped_lock ordersLock{ ordersMutex_ };

		if (!orders_.contains(order.GetOrderId()))
			return {};

		const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
		orderType = existingOrder->GetOrderType();
	}

	CancelOrder(order.GetOrderId());
	return AddOrder(order.ToOrderPointer(orderType));
}

/* Returns the size of the orderbook, i.e. the amount of orders.
 * Runs in O(1).
 */
std::size_t Orderbook::Size() const {
	std::scoped_lock ordersLock{ ordersMutex_ };
	return orders_.size();
}

/* Generates a snapshot of the aggregated orderbook based on the selected strategy.
 */
OrderbookLevelInfos Orderbook::GetOrderInfos(const IOrderbookSnapshotStrategy& strategy) const {
	return strategy.Generate(bids_, asks_);
}

OrderbookLevelInfos Orderbook::GetOrderInfos(const IOrderbookSnapshotStrategy& strategy, ThreadPool& pool) const {
	return strategy.Generate(bids_, asks_, pool);
}

/* Generates a snapshot of the aggregated orderbook, summarizing the total quantity at each price level for both bids and asks.
 * Runs in O(N) where N is the total amount of orders.
 */
OrderbookLevelInfos Orderbook::SequentialSnapshot::Generate(const BidMap& bids, const AskMap& asks) const {
	LevelInfos bidInfos;
	bidInfos.reserve(bids.size());
	for (const auto& [price, orderList] : bids) {
		Quantity total = 0;
		for (const auto& order : orderList)
			total += order->GetRemainingQuantity();
		bidInfos.push_back(LevelInfo{ price, total });
	}

	LevelInfos askInfos;
	askInfos.reserve(asks.size());
	for (const auto& [price, orderList] : asks) {
		Quantity total = 0;
		for (const auto& order : orderList)
			total += order->GetRemainingQuantity();
		askInfos.push_back(LevelInfo{ price, total });
	}

	return { bidInfos, askInfos };
}

/* Generates a snapshot of the aggregated orderbook, with the bids and asks being retrieved concurrently using async/futures.
 * Runs in O(N) where N is the total amount of orders.
 */
OrderbookLevelInfos Orderbook::AsyncSnapshot::Generate(const BidMap& bids, const AskMap& asks) const {
	auto CreateLevelInfos = [](Price price, const OrderPointers& orders) {
		return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
			[](Quantity runningSum, const OrderPointer& order)
			{ return runningSum + order->GetRemainingQuantity(); })
		};
	};

	auto bidsFuture = std::async(std::launch::async, [&]() {
		LevelInfos bidInfos;
		bidInfos.reserve(bids.size());
		for (const auto& [price, orders] : bids) {
			bidInfos.push_back(CreateLevelInfos(price, orders));
		}
		return bidInfos;
	});

	auto asksFuture = std::async(std::launch::async, [&]() {
		LevelInfos askInfos;
		askInfos.reserve(asks.size());
		for (const auto& [price, orders] : asks) {
			askInfos.push_back(CreateLevelInfos(price, orders));
		}
		return askInfos;
	});

	LevelInfos bidInfos = bidsFuture.get();
	LevelInfos askInfos = asksFuture.get();

	return OrderbookLevelInfos{ bidInfos, askInfos };
}

/* Generates a snapshot of the aggregated orderbook, with the bids and asks being retrieved concurrently using a thread pool.
 * Runs in O(N) where N is the total amount of orders.
 */
OrderbookLevelInfos Orderbook::ThreadPoolSnapshot::Generate(const BidMap& bids, const AskMap& asks, ThreadPool& pool) const {
	auto CreateLevelInfos = [](Price price, const OrderPointers& orders) {
		return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
			[](Quantity runningSum, const OrderPointer& order)
			{ return runningSum + order->GetRemainingQuantity(); })
		};
	};

	auto batchProcess = [&](auto beginIt, auto endIt) -> LevelInfos {
		LevelInfos localInfos;
		for (auto it = beginIt; it != endIt; ++it) {
			const auto& [price, orders] = *it;
			localInfos.push_back(CreateLevelInfos(price, orders));
		}
		return localInfos;
	};

	auto submitBatches = [&](const auto& container) -> LevelInfos {
		const size_t numElements = container.size();
		const size_t hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
		const size_t numBatches = std::min(hardwareThreads, numElements);
		size_t batchSize = numElements / numBatches;

		std::vector<std::future<LevelInfos>> futures;
		futures.reserve(numBatches);

		auto it = container.begin();
		for (size_t i = 0; i < numBatches; ++i) {
			auto batchStart = it;
			// Last batch takes any leftover elements.
			auto batchEnd = (i == numBatches - 1) ? container.end() : std::next(it, batchSize);
			futures.push_back(pool.submit(batchProcess, batchStart, batchEnd));
			it = batchEnd;
		}

		LevelInfos combinedInfos;
		combinedInfos.reserve(numElements);
		for (auto& fut : futures) {
			LevelInfos localInfos = fut.get();
			combinedInfos.insert(combinedInfos.end(),
				std::make_move_iterator(localInfos.begin()),
				std::make_move_iterator(localInfos.end()));
		}
		return combinedInfos;
	};

	LevelInfos bidInfos = submitBatches(bids);
	LevelInfos askInfos = submitBatches(asks);

	return OrderbookLevelInfos{ bidInfos, askInfos };
}

/* Generates a snapshot of the aggregated orderbook, with the bids and asks being retrieved concurrently using async/futures and a thread pool.
 * Runs in O(N) where N is the total amount of orders.
 */
OrderbookLevelInfos Orderbook::AsyncThreadPoolSnapshot::Generate(const BidMap& bids, const AskMap& asks, ThreadPool& pool) const {
	auto CreateLevelInfos = [](Price price, const OrderPointers& orders) {
		return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
			[](Quantity runningSum, const OrderPointer& order)
			{ return runningSum + order->GetRemainingQuantity(); })
		};
	};

	std::vector<std::future<LevelInfo>> bidFutures;
	bidFutures.reserve(bids.size());

	for (const auto& [price, orders] : bids) {
		bidFutures.push_back(pool.submit(CreateLevelInfos, price, orders));
	}

	LevelInfos bidInfos;
	bidInfos.reserve(bids.size());
	for (auto& fut : bidFutures) {
		bidInfos.push_back(fut.get());
	}

	std::vector<std::future<LevelInfo>> askFutures;
	askFutures.reserve(asks.size());

	for (const auto& [price, orders] : asks) {
		askFutures.push_back(pool.submit(CreateLevelInfos, price, orders));
	}

	LevelInfos askInfos;
	askInfos.reserve(asks.size());
	for (auto& fut : askFutures) {
		askInfos.push_back(fut.get());
	}

	return OrderbookLevelInfos{ bidInfos, askInfos };
}
