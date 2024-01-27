#include <cstdint>
#include <random>
#include <cstring> // memcpy
#include <compsky/macros/likely.hpp>
#include <cstdio>

std::random_device rd;
/* The state must be seeded so that it is not everywhere zero. */
uint64_t s[2] = { (uint64_t(rd()) << 32) ^ (rd()),
    (uint64_t(rd()) << 32) ^ (rd()) };
uint64_t curRand;
uint8_t bit = 63;

uint64_t xorshift128plus(){
	// TODO: Maybe possible to reduce to uint32_t
	uint64_t x = s[0];
	uint64_t const y = s[1];
	s[0] = y;
	x ^= x << 23; // a
	s[1] = x ^ y ^ (x >> 17) ^ (y >> 26); // b, c
	return s[1] + y;
}

extern "C"
uint32_t finding_0xedc72f12(const uint32_t* const arr_orig,  const unsigned arr_sz,  const unsigned shiftby,  unsigned max_iterations){
	uint32_t best_val = 0;
	uint32_t best = UINT32_MAX;
	
	uint32_t* const arr = reinterpret_cast<uint32_t*>(malloc(arr_sz*sizeof(uint32_t)));
	
	uint32_t prev_max_sz = 1024;
	uint32_t* counts_arr = reinterpret_cast<uint32_t*>(malloc(prev_max_sz*sizeof(uint32_t)));
	
	do {
		memcpy(arr, arr_orig, arr_sz*sizeof(uint32_t));
		
		const uint32_t val = (uint32_t)xorshift128plus();
		
		for (unsigned i = 0;  i < arr_sz;  ++i){
			arr[i] = ((arr[i] * val) & 0xffffffff) >> shiftby;
		}
		
		uint32_t max_idx = 0;
		for (unsigned i = 0;  i < arr_sz;  ++i){
			if (arr[i] > max_idx)
				max_idx = arr[i];
		}
		if (max_idx >= best)
			continue;
		
		if (unlikely(max_idx > prev_max_sz)){
			free(counts_arr);
			prev_max_sz = max_idx + 1;
			counts_arr = reinterpret_cast<uint32_t*>(malloc(prev_max_sz*sizeof(uint32_t)));
		}
		memset(counts_arr, 0, prev_max_sz*sizeof(uint32_t));
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
	
	uint32_t prev_max_sz = 1024;
	uint32_t* counts_arr = reinterpret_cast<uint32_t*>(malloc(prev_max_sz));
	
	do {
		memcpy(arr, arr_orig, arr_sz*sizeof(uint32_t));
		
		const uint32_t val = (uint32_t)xorshift128plus();
		
		for (unsigned i = 0;  i < arr_sz;  ++i){
			arr[i] = ((arr[i] * val) & 0xffffffff) >> shiftby;
		}
		
		uint32_t max_idx = 0;
		for (unsigned i = 0;  i < arr_sz;  ++i){
			if (arr[i] > max_idx)
				max_idx = arr[i];
		}
		if (max_idx >= best)
			continue;
		
		if (unlikely(max_idx > prev_max_sz)){
			free(counts_arr);
			prev_max_sz = max_idx + 1;
			counts_arr = reinterpret_cast<uint32_t*>(malloc(prev_max_sz*sizeof(uint32_t)));
		}
		memset(counts_arr, 0, prev_max_sz*sizeof(uint32_t));
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
