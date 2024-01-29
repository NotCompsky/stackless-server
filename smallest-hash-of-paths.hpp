#pragma once

#include <cstdint>

extern "C"
uint32_t finding_0xedc72f12(const uint32_t* const arr_orig,  const unsigned arr_sz,  const unsigned shiftby,  unsigned max_iterations);

extern "C"
uint32_t finding_0xedc72f12_w_avoids(const uint32_t* const arr_orig,  const uint32_t* const avoids,  const unsigned arr_sz,  const unsigned avoids_sz,  const unsigned shiftby,  unsigned max_iterations);

uint32_t find_max(const uint32_t* const arr,  const uint32_t arr_sz){
	uint32_t max = 0;
	for (uint32_t i = 0;  i < arr_sz;  ++i){
		if (arr[i] > max)
			max = arr[i];
	}
	return max;
}
