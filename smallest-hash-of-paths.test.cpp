#include <cstdint>
#include <cstdlib>
#include "smallest-hash-of-paths.hpp"

int main(){
	const uint32_t arr1_sz = 10;
	uint32_t* const arr1 = reinterpret_cast<uint32_t*>(malloc(arr1_sz*sizeof(uint32_t)));
	const uint32_t arr2_sz = 100;
	uint32_t* const arr2 = reinterpret_cast<uint32_t*>(malloc(arr2_sz*sizeof(uint32_t)));
	
	const unsigned shiftby1 = get_shiftby(arr1_sz) - 1; // so that less than half of the range is 'empty', making it easier to find space for the anti-inputs
	const unsigned shiftby2 = get_shiftby(arr2_sz);
	const unsigned max_iterations = 100000;
	
	const uint32_t multiplier1 = finding_0xedc72f12_w_avoids(arr1, arr2, arr1_sz, arr2_sz, shiftby1, max_iterations);
	
	free(arr2);
	free(arr1);
	
	return 0;
}
