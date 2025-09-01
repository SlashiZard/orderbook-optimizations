#include <iostream>
#include <iomanip>
#include <random>

#include "Benchmark.h"
#include "Orderbook.h"

namespace {
	constexpr OrderId INITIAL_ORDER_ID = 1;
	constexpr int RNG_SEED = 42;
	constexpr uint64_t PRICE_MIN = 30'000'000;
	constexpr uint64_t PRICE_MAX = 31'000'000;
	constexpr uint64_t QTY_MIN = 1;
	constexpr uint64_t QTY_MAX = 1000;
	constexpr double BUY_PROBABILITY = 0.5;
	
	constexpr double MS_TO_SEC = 1000.0;
	constexpr int OUTPUT_PRECISION = 8;
	constexpr size_t DEFAULT_BENCHMARK_SIZE = 100'000;
}

void prepareOrderbookBenchmark(size_t numOrders, Orderbook& orderbook) {
	OrderId orderId = INITIAL_ORDER_ID;

	std::mt19937 rng(RNG_SEED);
	std::uniform_int_distribution<uint64_t> priceDist(PRICE_MIN, PRICE_MAX);
	std::uniform_int_distribution<uint64_t> qtyDist(QTY_MIN, QTY_MAX);
	std::bernoulli_distribution sideDist(BUY_PROBABILITY);

	for (size_t i = 0; i < numOrders; ++i) {
		Side side = sideDist(rng) ? Side::Buy : Side::Sell;
		uint64_t price = priceDist(rng);
		uint64_t qty = qtyDist(rng);

		auto order = std::make_shared<Order>(OrderType::GoodTillCancel, orderId++, side, price, qty);
		orderbook.AddOrder(order);
	}
}

void runAddOrderBenchmark(size_t numOrders) {
	Orderbook orderbook;
	auto start = high_resolution_clock::now();
	prepareOrderbookBenchmark(numOrders, orderbook);

	auto end = high_resolution_clock::now();
	auto duration = duration_cast<milliseconds>(end - start).count();

	std::cout << "Processed " << numOrders << " orders in " << duration << "ms\n";
	std::cout << "Throughput: " << (numOrders * MS_TO_SEC / duration) << " orders/sec\n";
}

void runAllBenchmarks(ThreadPool& pool) {
	std::cout << std::fixed << std::setprecision(OUTPUT_PRECISION);

	runBenchmark("Orderbook::GetOrderInfos()", DEFAULT_BENCHMARK_SIZE, [](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::SequentialStrategy()); });
	runBenchmark("Orderbook::GetOrderInfosAsync()", DEFAULT_BENCHMARK_SIZE, [](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::AsyncStrategy()); });
	runBenchmark("Orderbook::GetOrderInfosAsyncPooled()", DEFAULT_BENCHMARK_SIZE, [&](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::AsyncThreadPoolStrategy(), pool); });
	runBenchmark("Orderbook::GetOrderInfosPooled()", DEFAULT_BENCHMARK_SIZE, [&](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::ThreadPoolStrategy(), pool); });
}
