#include <random>
#include <cstring> // memcpy
#include <compsky/macros/likely.hpp>
#include <cstdio>
#include "smallest-hash-of-paths.hpp"

std::random_device rd;
const uint64_t s[2] = {
	(uint64_t(rd()) << 32) ^ (rd()),
	(uint64_t(rd()) << 32) ^ (rd())
};

uint64_t xorshift128plus(){
	// TODO: Maybe possible to reduce to uint32_t
	uint64_t x = s[0];
	uint64_t const y = s[1];
	s[0] = y;
	x ^= x << 23; // a
	s[1] = x ^ y ^ (x >> 17) ^ (y >> 26); // b, c
	return s[1] + y;
}

uint32_t find_max(const uint32_t* const arr,  const uint32_t arr_sz){
	uint32_t max = 0;
	for (uint32_t i = 0;  i < arr_sz;  ++i){
		if (arr[i] > max)
			max = arr[i];
	}
	return max;
}

uint32_t get_shiftby(const uint32_t outputs_sz){
	uint32_t shiftby = 32;
	while (true){
		shiftby -= 1;
		if ((1 << (32-shiftby)) > outputs_sz){
			break;
		}
	}
	return shiftby;
}

extern "C"
uint32_t finding_0xedc72f12(const uint32_t* const arr_orig,  const unsigned arr_sz,  const unsigned shiftby,  unsigned max_iterations){
	uint32_t best_val = 0;
	uint32_t best = UINT32_MAX;
	
	uint32_t* const arr = reinterpret_cast<uint32_t*>(malloc(arr_sz*sizeof(uint32_t)));
	
	const uint32_t max_idx_plus_1 = 1 << (32 - shiftby);
	uint32_t* counts_arr = reinterpret_cast<uint32_t*>(malloc(max_idx_plus_1*sizeof(uint32_t)));
	
	do {
		memcpy(arr, arr_orig, arr_sz*sizeof(uint32_t));
		
		const uint32_t val = (uint32_t)xorshift128plus();
		
		for (unsigned i = 0;  i < arr_sz;  ++i){
			arr[i] = ((arr[i] * val) & 0xffffffff) >> shiftby;
		}
		
		const uint32_t max_idx = find_max(arr, arr_sz);
		if (max_idx >= best)
			continue;
		
		memset(counts_arr, 0, max_idx_plus_1*sizeof(uint32_t));
		for (unsigned i = 0;  i < arr_sz;  ++i){
			++counts_arr[arr[i]];
		}
		bool is_dupl = false;
		for (unsigned i = 0;  i <= max_idx;  ++i){
			if (counts_arr[i] > 1){
				is_dupl = true;
				break;
			}
		}
		if (is_dupl)
			continue;
		
		best = max_idx;
		best_val = val;
		printf("best max_indx==%u with multiplier %u (but desire max_indx==%u)\n", best, val, arr_sz-1);
	} while ((likely(best >= arr_sz)) and (likely(--max_iterations != 0)));
	
	free(arr);
	free(counts_arr);
	
	return best_val;
}

extern "C"
uint32_t finding_0xedc72f12_w_avoids(const uint32_t* const arr_orig,  const uint32_t* const avoids,  const unsigned arr_sz,  const unsigned avoids_sz,  const unsigned shiftby,  unsigned max_iterations){
	uint32_t best_val = 0;
	uint32_t best = UINT32_MAX;
	
	uint32_t* const arr = reinterpret_cast<uint32_t*>(malloc(arr_sz*sizeof(uint32_t)));
	
	const uint32_t max_idx_plus_1 = 1 << (32 - shiftby);
	uint32_t* counts_arr = reinterpret_cast<uint32_t*>(malloc(max_idx_plus_1*sizeof(uint32_t)));
	
	do {
		memcpy(arr, arr_orig, arr_sz*sizeof(uint32_t));
		
		const uint32_t val = (uint32_t)xorshift128plus();
		
		for (unsigned i = 0;  i < arr_sz;  ++i){
			arr[i] = ((arr[i] * val) & 0xffffffff) >> shiftby;
		}
		
		const uint32_t max_idx = find_max(arr, arr_sz);
		if (max_idx >= best)
			continue;
		
		memset(counts_arr, 0, max_idx_plus_1*sizeof(uint32_t));
		for (unsigned i = 0;  i < arr_sz;  ++i){
			++counts_arr[arr[i]];
		}
		bool is_dupl = false;
		for (unsigned i = 0;  i <= max_idx;  ++i){
			if (counts_arr[i] > 1){
				is_dupl = true;
				break;
			}
		}
		if (is_dupl)
			continue;
		
		bool must_continue = false;
		for (unsigned i = 0;  i < avoids_sz;  ++i){
			if ((((avoids[i] * val) & 0xffffffff) >> shiftby) <= max_idx){
				must_continue = true;
				break;
			}
		}
		if (must_continue)
			continue;
		
		best = max_idx;
		best_val = val;
		printf("best max_indx==%u with multiplier %u (but desire max_indx==%u)\n", best, val, arr_sz-1);
	} while ((likely(best >= arr_sz)) and (likely(--max_iterations != 0)));
	
	free(arr);
	free(counts_arr);
	
	return best_val;
}
