#pragma once

#include <string>
#include <vector>
#include <utility>

#include "LevelInfo.h"
#include "Orderbook.h"

struct L2Data {
	LevelInfos bids;
	LevelInfos asks;
	int64_t lastUpdateId;
};

class ApiClient {
public:
	L2Data FetchL2Data(const std::string& symbol, int limit = 5);
	void fillOrderbookBinance(Orderbook& orderbook, OrderId& orderId);
};
