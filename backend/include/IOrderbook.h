#pragma once

#include "Order.h"
#include "OrderModify.h"
#include "Trade.h"
#include "Usings.h"

#include <map>

using BidMap = std::map<Price, OrderPointers, std::greater<Price>>;
using AskMap = std::map<Price, OrderPointers, std::less<Price>>;

class IOrderbook {
public:
	virtual ~IOrderbook() = default;

	virtual Trades AddOrder(OrderPointer order) = 0;
	virtual void CancelOrder(OrderId orderId) = 0;
	virtual Trades ModifyOrder(OrderModify order) = 0;

	virtual std::size_t Size() const = 0;
};
