#include <iostream>
#include <iomanip>
#include <vector>

#include "Benchmark.h"
#include "Orderbook.h"
#include "VanillaOrderbook.h"
#include "BinarySearchOrderbook.h"

namespace {
	constexpr int OUTPUT_PRECISION = 8;
	constexpr size_t DEFAULT_BENCHMARK_SIZE = 200'000;

	constexpr int LABEL_COLUMN_WIDTH = 40;
	constexpr int ORDERS_COLUMN_WIDTH = 10;
	constexpr int DURATION_COLUMN_WIDTH = 12;
	constexpr int LEVELS_COLUMN_WIDTH = 10;
	constexpr int TABLE_WIDTH = LABEL_COLUMN_WIDTH + ORDERS_COLUMN_WIDTH + DURATION_COLUMN_WIDTH + LEVELS_COLUMN_WIDTH;

	void printResultsTable(const std::vector<BenchmarkResult>& results) {
		std::cout << std::left << std::setw(LABEL_COLUMN_WIDTH) << "Benchmark"
			<< std::right << std::setw(ORDERS_COLUMN_WIDTH) << "Orders"
			<< std::setw(DURATION_COLUMN_WIDTH) << "Time (ms)"
			<< std::setw(LEVELS_COLUMN_WIDTH) << "Levels" << '\n';
		std::cout << std::string(TABLE_WIDTH, '-') << '\n';

		for (const auto& result : results) {
			std::cout << std::left << std::setw(LABEL_COLUMN_WIDTH) << result.label
				<< std::right << std::setw(ORDERS_COLUMN_WIDTH) << result.numOrders
				<< std::setw(DURATION_COLUMN_WIDTH) << result.durationMs
				<< std::setw(LEVELS_COLUMN_WIDTH) << result.levels << '\n';
		}
	}
}

void runAllBenchmarks(ThreadPool& pool) {
	std::cout << std::fixed << std::setprecision(OUTPUT_PRECISION);

	std::vector<BenchmarkResult> results;
	results.push_back(runBenchmark<VanillaOrderbook>("VanillaOrderbook::GetOrderInfos()", DEFAULT_BENCHMARK_SIZE, [](VanillaOrderbook& ob) { return ob.GetOrderInfos(); }));
	results.push_back(runBenchmark<BinarySearchOrderbook>("BinarySearchOrderbook::GetOrderInfos()", DEFAULT_BENCHMARK_SIZE, [](BinarySearchOrderbook& ob) { return ob.GetOrderInfos(); }));
	results.push_back(runBenchmark<Orderbook>("Orderbook::GetOrderInfos()", DEFAULT_BENCHMARK_SIZE, [](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::SequentialStrategy()); }));
	results.push_back(runBenchmark<Orderbook>("Orderbook::GetOrderInfosAsync()", DEFAULT_BENCHMARK_SIZE, [](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::AsyncStrategy()); }));
	//results.push_back(runBenchmark<Orderbook>("Orderbook::GetOrderInfosAsyncPooled()", DEFAULT_BENCHMARK_SIZE, [&](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::AsyncThreadPoolStrategy(), pool); }));
	results.push_back(runBenchmark<Orderbook>("Orderbook::GetOrderInfosPooled()", DEFAULT_BENCHMARK_SIZE, [&](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::ThreadPoolStrategy(), pool); }));

	printResultsTable(results);
}
