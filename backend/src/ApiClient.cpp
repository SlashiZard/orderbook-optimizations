#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <iostream>

#include "ApiClient.h"
#include "Orderbook.h"

using json = nlohmann::json;

namespace {
	constexpr const char* BINANCE_API_URL = "https://api.binance.com/api/v3/depth?symbol=";
	constexpr const char* BINANCE_WS_URL = "wss://stream.binance.com:9443/ws/btcusdt@depth";
	constexpr const char* DEFAULT_TARGET_SYMBOL = "BTCUSDT";
	constexpr int HTTP_OK = 200;
	constexpr int DEFAULT_L2_LIMIT = 100;
}

L2Data ApiClient::FetchL2Data(const std::string& symbol, int limit) {
	std::string url = BINANCE_API_URL + symbol + "&limit=" + std::to_string(limit);

	cpr::Response r = cpr::Get(cpr::Url{ url });
	if (r.status_code != HTTP_OK) {
		throw std::runtime_error("HTTP request failed with error code " + std::to_string(r.status_code));
	}

	json j = json::parse(r.text);

	L2Data l2data;
	l2data.lastUpdateId = j["lastUpdateId"].get<int64_t>();

	for (const auto& bid : j["bids"]) {
		double priceDbl = std::stod(bid[0].get<std::string>());
		double quantityDbl = std::stod(bid[1].get<std::string>());

		l2data.bids.emplace_back(LevelInfo{
			static_cast<uint64_t>(priceDbl * SCALE_FACTOR + 0.5),
			static_cast<uint64_t>(quantityDbl * SCALE_FACTOR + 0.5)
		});
	}

	for (const auto& ask : j["asks"]) {
		double priceDbl = std::stod(ask[0].get<std::string>());
		double quantityDbl = std::stod(ask[1].get<std::string>());

		l2data.asks.emplace_back(LevelInfo{
			static_cast<uint64_t>(priceDbl * SCALE_FACTOR + 0.5),
			static_cast<uint64_t>(quantityDbl * SCALE_FACTOR + 0.5)
		});
	}

	return l2data;
}

void ApiClient::fillOrderbookBinance(Orderbook& orderbook, OrderId& orderId) {
	ApiClient apiClient;
	L2Data l2data = apiClient.FetchL2Data(std::string(DEFAULT_TARGET_SYMBOL), DEFAULT_L2_LIMIT);

	for (const auto& bid : l2data.bids) {
		double price = static_cast<double>(bid.price_) / SCALE_FACTOR;
		double quantity = static_cast<double>(bid.quantity_) / SCALE_FACTOR;
		std::cout << price << ' ' << quantity << '\n';

		orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId++, Side::Buy, bid.price_, bid.quantity_));
	}

	for (const auto& ask : l2data.asks) {
		double price = static_cast<double>(ask.price_) / SCALE_FACTOR;
		double quantity = static_cast<double>(ask.quantity_) / SCALE_FACTOR;
		std::cout << price << ' ' << quantity << '\n';

		orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId++, Side::Buy, ask.price_, ask.quantity_));
	}
}
