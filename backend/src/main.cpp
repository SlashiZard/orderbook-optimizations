#include "Orderbook.h"
#include "ApiClient.h"
#include "ThreadPool.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <iomanip>
#include <deque>
#include <mutex>
#include <random>
#include <chrono>

using namespace std::chrono;
using json = nlohmann::json;


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

template<typename Func>
void runBenchmark(const std::string& label, size_t numOrders, Func&& getLevelInfosFn) {
	Orderbook orderbook;
	prepareOrderbookBenchmark(numOrders, orderbook);

	auto start = high_resolution_clock::now();

	OrderbookLevelInfos levelInfos = getLevelInfosFn(orderbook);

	auto end = high_resolution_clock::now();
	auto duration = duration_cast<milliseconds>(end - start).count();

	std::cout << "Executed " << label << " of an orderbook with " << numOrders << " elements in " << duration << "ms\n";
	std::cout << "Levels: " << levelInfos.GetBids().size() + levelInfos.GetAsks().size() << "\n";
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


int main() {
	ThreadPool pool(std::thread::hardware_concurrency());

	std::cout << std::fixed << std::setprecision(8);

	runBenchmark("Orderbook::GetOrderInfos()", 100'000, [](Orderbook& ob) { return ob.GetOrderInfos(); });
	runBenchmark("Orderbook::GetOrderInfosAsync()", 100'000, [](Orderbook& ob) { return ob.GetOrderInfosAsync(); });
	runBenchmark("Orderbook::GetOrderInfosAsyncPooled()", 100'000, [&](Orderbook& ob) { return ob.GetOrderInfosAsyncPooled(pool); });
	runBenchmark("Orderbook::GetOrderInfosPooled()", 100'000, [&](Orderbook& ob) { return ob.GetOrderInfosPooled(pool); });
	
	return 0;
}
