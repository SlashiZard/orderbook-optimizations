#include <iostream>
#include <iomanip>

#include "Benchmark.h"
#include "Orderbook.h"
#include "VanillaOrderbook.h"

namespace {
	constexpr int OUTPUT_PRECISION = 8;
	constexpr size_t DEFAULT_BENCHMARK_SIZE = 100'000;
}

void runAllBenchmarks(ThreadPool& pool) {
	std::cout << std::fixed << std::setprecision(OUTPUT_PRECISION);

	runBenchmark<VanillaOrderbook>("VanillaOrderbook::GetOrderInfos()", DEFAULT_BENCHMARK_SIZE, [](VanillaOrderbook& ob) { return ob.GetOrderInfos(); });
	runBenchmark<Orderbook>("Orderbook::GetOrderInfos()", DEFAULT_BENCHMARK_SIZE, [](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::SequentialStrategy()); });
	runBenchmark<Orderbook>("Orderbook::GetOrderInfosAsync()", DEFAULT_BENCHMARK_SIZE, [](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::AsyncStrategy()); });
	runBenchmark<Orderbook>("Orderbook::GetOrderInfosAsyncPooled()", DEFAULT_BENCHMARK_SIZE, [&](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::AsyncThreadPoolStrategy(), pool); });
	runBenchmark<Orderbook>("Orderbook::GetOrderInfosPooled()", DEFAULT_BENCHMARK_SIZE, [&](Orderbook& ob) { return ob.GetOrderInfos(Orderbook::ThreadPoolStrategy(), pool); });
}
