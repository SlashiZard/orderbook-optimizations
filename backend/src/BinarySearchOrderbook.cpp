#include <numeric>
#include <chrono>
#include <ctime>
#include <future>
#include <iostream>

#include "BinarySearchOrderbook.h"

/* Cancels all orders with the given order ids.
 * Runs in O(N^2), where N is the amount of given order ids.
 */
void BinarySearchOrderbook::CancelOrders(OrderIds orderIds) {
	for (const auto& orderId : orderIds)
		CancelOrderInternal(orderId);
}

/* Cancels the order with the given order id.
 * Runs in O(N) where N is the amount of orders.
 */
void BinarySearchOrderbook::CancelOrderInternal(OrderId orderId) {
	askOrders_.erase(
		std::remove_if(askOrders_.begin(), askOrders_.end(), [&](const auto& o) {
			return o->GetOrderId() == orderId;
			}),
		askOrders_.end()
	);

	bidOrders_.erase(
		std::remove_if(bidOrders_.begin(), bidOrders_.end(), [&](const auto& o) {
			return o->GetOrderId() == orderId;
			}),
		bidOrders_.end()
	);
}

/* Retrieves the order with the best ask given price-time priority or nullptr if there are none.
 * Runs in O(N) where N is the amount of orders.
 */
const OrderPointer BinarySearchOrderbook::getBestAsk() const {
	OrderPointer bestAskOrder = nullptr;

	for (const auto& order : orders_) {
		if (!order || order->GetSide() != Side::Sell || order->GetRemainingQuantity() == 0) continue;

		if (!bestAskOrder || order->GetPrice() < bestAskOrder->GetPrice()) {
			bestAskOrder = order;
		}
	}

	return bestAskOrder;
}

/* Retrieves the order with the best bid given price-time priority or nullptr if there are none.
 * Runs in O(N) where N is the amount of orders.
 */
const OrderPointer BinarySearchOrderbook::getBestBid() const {
	OrderPointer bestBidOrder = nullptr;

	for (const auto& order : orders_) {
		if (!order || order->GetSide() != Side::Buy || order->GetRemainingQuantity() == 0) continue;

		if (!bestBidOrder || order->GetPrice() > bestBidOrder->GetPrice()) {
			bestBidOrder = order;
		}
	}

	return bestBidOrder;
}

/* Retrieves the order with the best ask given price-time priority or nullptr if there are none.
 * Runs in O(N) where N is the amount of orders.
 */
const OrderPointer BinarySearchOrderbook::getWorstAsk() const {
	OrderPointer bestAskOrder = nullptr;

	for (const auto& order : orders_) {
		if (!order || order->GetSide() != Side::Sell || order->GetRemainingQuantity() == 0) continue;

		if (!bestAskOrder || order->GetPrice() > bestAskOrder->GetPrice()) {
			bestAskOrder = order;
		}
	}

	return bestAskOrder;
}

/* Retrieves the order with the best bid given price-time priority or nullptr if there are none.
 * Runs in O(N) where N is the amount of orders.
 */
const OrderPointer BinarySearchOrderbook::getWorstBid() const {
	OrderPointer bestBidOrder = nullptr;

	for (const auto& order : orders_) {
		if (!order || order->GetSide() != Side::Buy || order->GetRemainingQuantity() == 0) continue;

		if (!bestBidOrder || order->GetPrice() < bestBidOrder->GetPrice()) {
			bestBidOrder = order;
		}
	}

	return bestBidOrder;
}

bool BinarySearchOrderbook::orderExists(OrderId orderId) const {
	return std::any_of(orders_.begin(), orders_.end(), [&](const OrderPointer& o) {
		return o && o->GetOrderId() == orderId;
		});
}

bool BinarySearchOrderbook::CanMatch(Side side, Price price) const {
	if (orders_.empty())
		return false;

	if (side == Side::Buy) {
		const auto& bestAskOrder = getBestAsk();
		return price >= bestAskOrder->GetPrice();
	}
	else {
		const auto& bestBidOrder = getBestBid();
		return price <= bestBidOrder->GetPrice();
	}
}

/* Checks if an order with the given side, price, and quantity can be fully filled.
 * Runs in O(N), where N is the amount of price levels.
 */
bool BinarySearchOrderbook::CanFullyFill(Side side, Price price, Quantity quantity) const {
	for (const auto& order : orders_) {
		if (!order || order->GetSide() != (side == Side::Buy ? Side::Sell : Side::Buy)) continue;
		if (order->GetRemainingQuantity() == 0) continue;

		if ((side == Side::Buy && order->GetPrice() > price) ||
			(side == Side::Sell && order->GetPrice() < price))
			continue;

		if (quantity <= order->GetRemainingQuantity())
			return true;

		quantity -= order->GetRemainingQuantity();
	}

	return false;
}

/* Matches orders in the orderbook.
 * Runs in O(N * log(M)) where N is the total amount of orders and M is the amount of price levels.
 */
Trades BinarySearchOrderbook::MatchOrders() {
	Trades trades;

	while (true) {
		OrderPointer bestBid = nullptr;
		OrderPointer bestAsk = nullptr;

		for (auto& order : orders_) {
			if (!order || order->GetSide() != Side::Buy || order->GetRemainingQuantity() == 0) continue;

			if (!bestBid || order->GetPrice() > bestBid->GetPrice()) {
				bestBid = order;
			}
		}

		for (auto& order : orders_) {
			if (!order || order->GetSide() != Side::Sell || order->GetRemainingQuantity() == 0) continue;

			if (!bestAsk || order->GetPrice() < bestAsk->GetPrice()) {
				bestAsk = order;
			}
		}

		if (!bestBid || !bestAsk || bestBid->GetPrice() < bestAsk->GetPrice()) break;

		Quantity quantity = std::min(bestBid->GetRemainingQuantity(), bestAsk->GetRemainingQuantity());

		bestBid->Fill(quantity);
		bestAsk->Fill(quantity);

		//std::cout << "Matched orders " << bestBid->GetOrderId() << " and " << bestAsk->GetOrderId() << '\n';

		trades.push_back(Trade{
			TradeInfo{ bestBid->GetOrderId(), bestBid->GetPrice(), quantity },
			TradeInfo{ bestAsk->GetOrderId(), bestAsk->GetPrice(), quantity }
			});

		orders_.erase(
			std::remove_if(orders_.begin(), orders_.end(), [](const auto& o) {
				return o->IsFilled();
				}),
			orders_.end()
		);
	}

	return trades;
}

/* Adds an order to the orderbook.
 * Runs in O(N * log(M)) where N is the total amount of orders and M is the amount of price levels.
 */
Trades BinarySearchOrderbook::AddOrder(OrderPointer order) {
	if (!order || orderExists(order->GetOrderId()))
		return {};

	if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
		return {};

	if (order->GetOrderType() == OrderType::Market) {
		if (order->GetSide() == Side::Buy) {
			const auto& worstAskOrder = getWorstAsk();
			if (!worstAskOrder) return {};
			order->ToGoodTillCancel(worstAskOrder->GetPrice());
		}
		else if (order->GetSide() == Side::Sell) {
			const auto& worstBidOrder = getWorstBid();
			if (!worstBidOrder) return {};
			order->ToGoodTillCancel(worstBidOrder->GetPrice());
		}
		else
			return {};
	}

	if (order->GetOrderType() == OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetInitialQuantity()))
		return {};

	orders_.push_back(order);

	return MatchOrders();
}

/* Acquires a lock on the orders and then cancels the order with the given order id.
 * Runs in O(log(M)) where M is the number of distinct price levels.
 */
void BinarySearchOrderbook::CancelOrder(OrderId orderId) {
	CancelOrderInternal(orderId);
}

/* Modifies the order with the given order id by first cancelling the order, and then adding a new order with the modified data.
 * Runs in O(N * log(M)) where N is the total amount of orders and M is the amount of price levels.
 */
Trades BinarySearchOrderbook::ModifyOrder(OrderModify order) {
	OrderType orderType;

	auto it = std::find_if(orders_.begin(), orders_.end(), [&](const auto& o) {
		return o->GetOrderId() == order.GetOrderId();
		});
	if (it == orders_.end()) return {};

	orderType = (*it)->GetOrderType();

	CancelOrder(order.GetOrderId());
	return AddOrder(order.ToOrderPointer(orderType));
}

/* Returns the size of the orderbook, i.e. the amount of orders.
 * Runs in O(1).
 */
std::size_t BinarySearchOrderbook::Size() const {
	return orders_.size();
}

/* Generates a snapshot of the aggregated orderbook based on the selected strategy.
 */
OrderbookLevelInfos BinarySearchOrderbook::GetOrderInfos() const {
	std::map<Price, Quantity, std::greater<Price>> bidTotalMap;
	std::map<Price, Quantity, std::less<Price>> askTotalMap;

	for (const auto& order : orders_) {
		if (!order || order->GetRemainingQuantity() == 0) continue;

		auto price = order->GetPrice();
		auto quantity = order->GetRemainingQuantity();

		if (order->GetSide() == Side::Buy) {
			bidTotalMap[price] += quantity;
		}
		else {
			askTotalMap[price] += quantity;
		}
	}

	LevelInfos bidInfos, askInfos;
	for (const auto& [price, total] : bidTotalMap) {
		bidInfos.push_back(LevelInfo{ price, total });
	}

	for (const auto& [price, total] : askTotalMap) {
		askInfos.push_back(LevelInfo{ price, total });
	}

	return { bidInfos, askInfos };
}
