#pragma once

#include <cstddef>
#include <chrono>
#include <string>
#include <iostream>

#include "Orderbook.h"

using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

void prepareOrderbookBenchmark(size_t numOrders, Orderbook& orderbook);

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

void runAddOrderBenchmark(size_t numOrders);

void runAllBenchmarks(ThreadPool& pool);
