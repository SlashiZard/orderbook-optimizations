#pragma once

#include <cstddef>
#include <chrono>
#include <string>
#include <iostream>
#include <random>

#include "Orderbook.h"

using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

namespace {
	constexpr OrderId INITIAL_ORDER_ID = 1;
	constexpr int RNG_SEED = 42;
	constexpr uint64_t PRICE_MIN = 30'000'000;
	constexpr uint64_t PRICE_MAX = 31'000'000;
	constexpr uint64_t QTY_MIN = 1;
	constexpr uint64_t QTY_MAX = 1000;
	constexpr double BUY_PROBABILITY = 0.5;
	constexpr double MS_TO_SEC = 1000.0;
}

template <typename OrderbookType>
void prepareOrderbookBenchmark(size_t numOrders, OrderbookType& orderbook) {
	OrderId orderId = INITIAL_ORDER_ID;

	std::mt19937 rng(RNG_SEED);
	std::uniform_int_distribution<uint64_t> priceDist(PRICE_MIN, PRICE_MAX);
	std::uniform_int_distribution<uint64_t> qtyDist(QTY_MIN, QTY_MAX);
	std::bernoulli_distribution sideDist(BUY_PROBABILITY);

	for (size_t i = 0; i < numOrders; ++i) {
		Side side = sideDist(rng) ? Side::Buy : Side::Sell;
		uint64_t price = priceDist(rng);
		uint64_t qty = qtyDist(rng);

		//std::cout << "Adding order id " << orderId << " " << (side == Side::Buy ? "buy" : "sell") << " " << price << " " << qty << '\n';
		auto order = std::make_shared<Order>(OrderType::GoodTillCancel, orderId++, side, price, qty);
		
		orderbook.AddOrder(order);
	}
}

template<typename OrderbookType, typename Func>
void runBenchmark(const std::string& label, size_t numOrders, Func&& getLevelInfosFn) {
	OrderbookType orderbook;
	prepareOrderbookBenchmark(numOrders, orderbook);

	auto start = high_resolution_clock::now();

	OrderbookLevelInfos levelInfos = getLevelInfosFn(orderbook);

	auto end = high_resolution_clock::now();
	auto duration = duration_cast<milliseconds>(end - start).count();

	std::cout << "Executed " << label << " of an orderbook with " << numOrders << " elements in " << duration << "ms\n";
	std::cout << "Levels: " << levelInfos.GetBids().size() + levelInfos.GetAsks().size() << "\n";
}

template <typename OrderbookType>
void runAddOrderBenchmark(size_t numOrders) {
	OrderbookType orderbook;
	auto start = high_resolution_clock::now();
	prepareOrderbookBenchmark<OrderbookType>(numOrders, orderbook);

	auto end = high_resolution_clock::now();
	auto duration = duration_cast<milliseconds>(end - start).count();

	std::cout << "Processed " << numOrders << " orders in " << duration << "ms\n";
	std::cout << "Throughput: " << (numOrders * MS_TO_SEC / duration) << " orders/sec\n";
}

void runAllBenchmarks(ThreadPool& pool);
