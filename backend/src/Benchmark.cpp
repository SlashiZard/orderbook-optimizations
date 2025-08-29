#include <iostream>
#include <iomanip>
#include <random>

#include "Benchmark.h"
#include "Orderbook.h"

void prepareOrderbookBenchmark(size_t numOrders, Orderbook& orderbook) {
	const int SEED = 42;
	OrderId orderId = 1;

	std::mt19937 rng(SEED);
	std::uniform_int_distribution<uint64_t> priceDist(30'000'000, 31'000'000);
	std::uniform_int_distribution<uint64_t> qtyDist(1, 1000);
	std::bernoulli_distribution sideDist(0.5);

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
	std::cout << "Throughput: " << (numOrders * 1000.0 / duration) << " orders/sec\n";
}

void runAllBenchmarks(ThreadPool& pool) {
	std::cout << std::fixed << std::setprecision(8);

	runBenchmark("Orderbook::GetOrderInfos()", 100'000, [](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::SequentialStrategy()); });
	runBenchmark("Orderbook::GetOrderInfosAsync()", 100'000, [](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::AsyncStrategy()); });
	runBenchmark("Orderbook::GetOrderInfosAsyncPooled()", 100'000, [&](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::AsyncThreadPoolStrategy(), pool); });
	runBenchmark("Orderbook::GetOrderInfosPooled()", 100'000, [&](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::ThreadPoolStrategy(), pool); });
}
