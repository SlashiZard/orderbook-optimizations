#pragma once

#include <vector>

const uint64_t SCALE_FACTOR = 100'000'000;
using Price = std::uint64_t;
using Quantity = std::uint64_t;
using OrderId = std::uint64_t;
using OrderIds = std::vector<OrderId>;