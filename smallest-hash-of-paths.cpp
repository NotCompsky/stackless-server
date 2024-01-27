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
	uint32_t val = 0;
	uint32_t best = -1;
	
	uint32_t* const arr = reinterpret_cast<uint32_t*>(malloc(arr_sz*sizeof(uint32_t)));
	
	do {
		memcpy(arr, arr_orig, arr_sz*sizeof(uint32_t));
		
		val = (uint32_t)xorshift128plus();
		
		for (unsigned i = 0;  i < arr_sz;  ++i){
			arr[i] = ((arr[i] * val) & 0xffffffff) >> shiftby;
		}
		
		bool abandon = false;
		for (unsigned i = 0;  i < arr_sz;  ++i){
			if (arr[i] > 2*arr_sz){
				abandon = true;
				break;
			}
		}
		if (abandon)
			continue;
		
		uint32_t nonunique_values = -1;
		for (unsigned i = 0;  i < arr_sz-1;  ++i){
			for (unsigned j = i+1;  j < arr_sz;  ++j){
				if (unlikely(arr[i] == arr[j])){
					nonunique_values = arr[i];
					break;
				}
			}
			if (nonunique_values != -1)
				break;
		}
		if (likely(nonunique_values != -1))
			continue;
		
		uint32_t max_idx = 0;
		for (unsigned i = 0;  i < arr_sz;  ++i){
			if (arr[i] > max_idx)
				max_idx = arr[i];
		}
		
		if (max_idx < best){
			best = max_idx;
			printf("best max_indx==%u with multiplier %u (but desire max_indx==%u)\n", best, val, arr_sz-1);
		}
		if (--max_iterations == 0)
			break;
	} while (likely(best >= arr_sz));
	
	return val;
}

extern "C"
int finding_0xedc72f12_w_avoids(const uint32_t* const arr_orig,  const uint32_t* const avoids,  const unsigned arr_sz,  const unsigned avoids_sz,  const unsigned shiftby,  unsigned max_iterations){
	uint32_t val = 0;
	uint32_t best = -1;
	
	uint32_t* const arr = reinterpret_cast<uint32_t*>(malloc(arr_sz*sizeof(uint32_t)));
	
	do {
		memcpy(arr, arr_orig, arr_sz*sizeof(uint32_t));
		
		val = (uint32_t)xorshift128plus();
		
		for (unsigned i = 0;  i < arr_sz;  ++i){
			arr[i] = ((arr[i] * val) & 0xffffffff) >> shiftby;
		}
		
		bool abandon = false;
		for (unsigned i = 0;  i < arr_sz;  ++i){
			if (arr[i] > 2*arr_sz){
				abandon = true;
				break;
			}
		}
		if (abandon)
			continue;
		
		uint32_t nonunique_values = -1;
		for (unsigned i = 0;  i < arr_sz-1;  ++i){
			for (unsigned j = i+1;  j < arr_sz;  ++j){
				if (unlikely(arr[i] == arr[j])){
					nonunique_values = arr[i];
					break;
				}
			}
			if (nonunique_values != -1)
				break;
		}
		if (likely(nonunique_values != -1))
			continue;
		
		uint32_t max_idx = 0;
		for (unsigned i = 0;  i < arr_sz;  ++i){
			if (arr[i] > max_idx)
				max_idx = arr[i];
		}
		
		bool must_continue = false;
		for (unsigned i = 0;  i < avoids_sz;  ++i){
			if ((((avoids[i] * val) & 0xffffffff) >> shiftby) < max_idx+1){
				must_continue = true;
				break;
			}
		}
		if (must_continue)
			continue;
		
		if (max_idx < best){
			best = max_idx;
			printf("best max_indx==%u with multiplier %u (but desire max_indx==%u)\n", best, val, arr_sz-1);
		}
		if (--max_iterations == 0)
			break;
	} while (likely(best >= arr_sz));
	
	return val;
}
