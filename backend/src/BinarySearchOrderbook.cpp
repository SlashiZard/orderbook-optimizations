#include <algorithm>
#include <numeric>

#include "BinarySearchOrderbook.h"

namespace {
	/* askOrders_ is sorted ascending by price, so the first order whose price is
	 * strictly greater than `price` marks the end of the eligible (price <= `price`) prefix.
	 * Templated on the vector's const-ness so it can locate both a mutable insertion point
	 * (AddOrder) and a read-only prefix boundary (CanFullyFill).
	 */
	template <typename AskVector>
	auto AskInsertionPoint(AskVector& askOrders, Price price) {
		return std::upper_bound(askOrders.begin(), askOrders.end(), price, [](Price p, const OrderPointer& o) {
			return p < o->GetPrice();
			});
	}

	/* bidOrders_ is sorted descending by price, so the first order whose price is
	 * strictly less than `price` marks the end of the eligible (price >= `price`) prefix.
	 */
	template <typename BidVector>
	auto BidInsertionPoint(BidVector& bidOrders, Price price) {
		return std::upper_bound(bidOrders.begin(), bidOrders.end(), price, [](Price p, const OrderPointer& o) {
			return p > o->GetPrice();
			});
	}
}

/* Cancels all orders with the given order ids.
 * Runs in O(N * M), where N is the amount of given order ids and M is the amount of orders.
 */
void BinarySearchOrderbook::CancelOrders(OrderIds orderIds) {
	for (const auto& orderId : orderIds)
		CancelOrderInternal(orderId);
}

/* Cancels the order with the given order id.
 * Runs in O(M) where M is the total amount of orders, since a plain vector has no order-id index.
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

/* Retrieves the ask order with the best (lowest) price, or nullptr if there are none.
 * Runs in O(1), since askOrders_ is kept sorted ascending by price.
 */
const OrderPointer BinarySearchOrderbook::getBestAsk() const {
	return askOrders_.empty() ? nullptr : askOrders_.front();
}

/* Retrieves the bid order with the best (highest) price, or nullptr if there are none.
 * Runs in O(1), since bidOrders_ is kept sorted descending by price.
 */
const OrderPointer BinarySearchOrderbook::getBestBid() const {
	return bidOrders_.empty() ? nullptr : bidOrders_.front();
}

/* Retrieves the ask order with the worst (highest) price, or nullptr if there are none.
 * Runs in O(1), since askOrders_ is kept sorted ascending by price.
 */
const OrderPointer BinarySearchOrderbook::getWorstAsk() const {
	return askOrders_.empty() ? nullptr : askOrders_.back();
}

/* Retrieves the bid order with the worst (lowest) price, or nullptr if there are none.
 * Runs in O(1), since bidOrders_ is kept sorted descending by price.
 */
const OrderPointer BinarySearchOrderbook::getWorstBid() const {
	return bidOrders_.empty() ? nullptr : bidOrders_.back();
}

/* Runs in O(M) where M is the total amount of orders, since a plain vector has no order-id index.
 */
bool BinarySearchOrderbook::orderExists(OrderId orderId) const {
	return std::any_of(askOrders_.begin(), askOrders_.end(), [&](const OrderPointer& o) {
		return o->GetOrderId() == orderId;
		})
		|| std::any_of(bidOrders_.begin(), bidOrders_.end(), [&](const OrderPointer& o) {
			return o->GetOrderId() == orderId;
			});
}

/* Runs in O(1), using the O(1) best-price lookups above.
 */
bool BinarySearchOrderbook::CanMatch(Side side, Price price) const {
	if (side == Side::Buy) {
		const auto& bestAskOrder = getBestAsk();
		return bestAskOrder && price >= bestAskOrder->GetPrice();
	}
	else {
		const auto& bestBidOrder = getBestBid();
		return bestBidOrder && price <= bestBidOrder->GetPrice();
	}
}

/* Checks if an order with the given side, price, and quantity can be fully filled.
 * Runs in O(log M + K), where M is the amount of orders on the opposing side and K is the
 * number of price-eligible orders inspected: std::upper_bound finds the eligible prefix in
 * O(log M), then quantities are summed only within that prefix.
 */
bool BinarySearchOrderbook::CanFullyFill(Side side, Price price, Quantity quantity) const {
	if (side == Side::Buy) {
		auto end = AskInsertionPoint(askOrders_, price);
		for (auto it = askOrders_.begin(); it != end; ++it) {
			if (quantity <= (*it)->GetRemainingQuantity())
				return true;
			quantity -= (*it)->GetRemainingQuantity();
		}
	}
	else {
		auto end = BidInsertionPoint(bidOrders_, price);
		for (auto it = bidOrders_.begin(); it != end; ++it) {
			if (quantity <= (*it)->GetRemainingQuantity())
				return true;
			quantity -= (*it)->GetRemainingQuantity();
		}
	}

	return false;
}

/* Matches orders in the orderbook.
 * Runs in O(T) where T is the total quantity matched across all trades, since each match
 * removes a fully-filled order from the front of its vector in amortized O(1) relative to
 * the remaining matching work.
 */
Trades BinarySearchOrderbook::MatchOrders() {
	Trades trades;

	while (!bidOrders_.empty() && !askOrders_.empty()) {
		auto& bestBid = bidOrders_.front();
		auto& bestAsk = askOrders_.front();

		if (bestBid->GetPrice() < bestAsk->GetPrice()) break;

		Quantity quantity = std::min(bestBid->GetRemainingQuantity(), bestAsk->GetRemainingQuantity());

		bestBid->Fill(quantity);
		bestAsk->Fill(quantity);

		trades.push_back(Trade{
			TradeInfo{ bestBid->GetOrderId(), bestBid->GetPrice(), quantity },
			TradeInfo{ bestAsk->GetOrderId(), bestAsk->GetPrice(), quantity }
			});

		if (bestBid->IsFilled()) bidOrders_.erase(bidOrders_.begin());
		if (bestAsk->IsFilled()) askOrders_.erase(askOrders_.begin());
	}

	return trades;
}

/* Adds an order to the orderbook, inserting it at its sorted position.
 * Runs in O(log M + M) where M is the amount of orders on its side: O(log M) to find the
 * sorted insertion point via binary search, O(M) to shift elements for the vector insert.
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

	if (order->GetSide() == Side::Sell) {
		askOrders_.insert(AskInsertionPoint(askOrders_, order->GetPrice()), order);
	}
	else {
		bidOrders_.insert(BidInsertionPoint(bidOrders_, order->GetPrice()), order);
	}

	return MatchOrders();
}

/* Cancels the order with the given order id.
 * Runs in O(M) where M is the total amount of orders.
 */
void BinarySearchOrderbook::CancelOrder(OrderId orderId) {
	CancelOrderInternal(orderId);
}

/* Modifies the order with the given order id by first cancelling the order, and then adding a new order with the modified data.
 * Runs in O(M) to find and cancel the existing order, plus the cost of AddOrder for the replacement.
 */
Trades BinarySearchOrderbook::ModifyOrder(OrderModify order) {
	auto find = [&](const std::vector<OrderPointer>& orders) {
		return std::find_if(orders.begin(), orders.end(), [&](const auto& o) {
			return o->GetOrderId() == order.GetOrderId();
			});
		};

	auto askIt = find(askOrders_);
	auto bidIt = find(bidOrders_);

	OrderType orderType;
	if (askIt != askOrders_.end()) orderType = (*askIt)->GetOrderType();
	else if (bidIt != bidOrders_.end()) orderType = (*bidIt)->GetOrderType();
	else return {};

	CancelOrder(order.GetOrderId());
	return AddOrder(order.ToOrderPointer(orderType));
}

/* Returns the size of the orderbook, i.e. the amount of orders.
 * Runs in O(1).
 */
std::size_t BinarySearchOrderbook::Size() const {
	return askOrders_.size() + bidOrders_.size();
}

/* Generates a snapshot of the aggregated orderbook.
 * Runs in O(M) where M is the total amount of orders.
 */
OrderbookLevelInfos BinarySearchOrderbook::GetOrderInfos() const {
	std::map<Price, Quantity, std::greater<Price>> bidTotalMap;
	std::map<Price, Quantity, std::less<Price>> askTotalMap;

	for (const auto& order : bidOrders_) {
		if (order->GetRemainingQuantity() == 0) continue;
		bidTotalMap[order->GetPrice()] += order->GetRemainingQuantity();
	}

	for (const auto& order : askOrders_) {
		if (order->GetRemainingQuantity() == 0) continue;
		askTotalMap[order->GetPrice()] += order->GetRemainingQuantity();
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
