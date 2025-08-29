#pragma once

#include "Order.h"
#include "OrderModify.h"
#include "Trade.h"
#include "OrderbookLevelInfos.h"

class IOrderBook {
public:
	virtual ~IOrderBook() = default;

	virtual Trades AddOrder(OrderPointer order) = 0;
	virtual void CancelOrder(OrderId orderId);
	virtual Trades ModifyOrder(OrderModify order);

	virtual std::size_t Size() const = 0;


};
