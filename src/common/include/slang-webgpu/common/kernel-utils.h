#pragma once

#include <variant>
#include <cstdint>

// Utility types shared by all generated Kernel classes

// When specifying a dispatch size, we do so either as a number of workgroups,
// or a number of threads (rounded up to the next round number of workgroups)
struct WorkgroupCount {
	uint32_t x = 1;
	uint32_t y = 1;
	uint32_t z = 1;
};
struct ThreadCount {
	uint32_t x = 1;
	uint32_t y = 1;
	uint32_t z = 1;
};
using DispatchSize = std::variant<WorkgroupCount, ThreadCount>;

inline uint32_t divideAndCeil(uint32_t x, uint32_t y) {
	return (x + y - 1) / y;
}
