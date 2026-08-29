#include <iostream>
#include <iomanip>

#include "Benchmark.h"
#include "MapOrderbook.h"
#include "VanillaOrderbook.h"
#include "BinarySearchOrderbook.h"

namespace {
	constexpr int OUTPUT_PRECISION = 8;
	constexpr size_t DEFAULT_BENCHMARK_SIZE = 200'000;
}

void runAllBenchmarks(ThreadPool& pool) {
	std::cout << std::fixed << std::setprecision(OUTPUT_PRECISION);

	runBenchmark<VanillaOrderbook>("VanillaOrderbook::GetOrderInfos()", DEFAULT_BENCHMARK_SIZE, [](VanillaOrderbook& ob) { return ob.GetOrderInfos(); });
	runBenchmark<BinarySearchOrderbook>("BinarySearchOrderbook::GetOrderInfos()", DEFAULT_BENCHMARK_SIZE, [](BinarySearchOrderbook& ob) { return ob.GetOrderInfos(); });
	runBenchmark<MapOrderbook>("MapOrderbook::GetOrderInfos()", DEFAULT_BENCHMARK_SIZE, [](MapOrderbook& ob) { return ob.GetOrderInfos(MapOrderbook::SequentialStrategy()); });
	runBenchmark<MapOrderbook>("MapOrderbook::GetOrderInfosAsync()", DEFAULT_BENCHMARK_SIZE, [](MapOrderbook& ob) { return ob.GetOrderInfos(MapOrderbook::AsyncStrategy()); });
	//runBenchmark<MapOrderbook>("MapOrderbook::GetOrderInfosAsyncPooled()", DEFAULT_BENCHMARK_SIZE, [&](MapOrderbook& ob) { return ob.GetOrderInfos(MapOrderbook::AsyncThreadPoolStrategy(), pool); });
	runBenchmark<MapOrderbook>("MapOrderbook::GetOrderInfosPooled()", DEFAULT_BENCHMARK_SIZE, [&](MapOrderbook& ob) { return ob.GetOrderInfos(MapOrderbook::ThreadPoolStrategy(), pool); });
}
