#include "ThreadPool.h"
#include "Benchmark.h"

int main() {
	ThreadPool pool(std::thread::hardware_concurrency());
	runAllBenchmarks(pool);
	return 0;
}
