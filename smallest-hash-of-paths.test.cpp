#include <cstdint>
#include <cstdlib>

extern "C"
uint32_t finding_0xedc72f12(const uint32_t* const arr_orig,  const unsigned arr_sz,  const unsigned shiftby,  unsigned max_iterations);

extern "C"
uint32_t finding_0xedc72f12_w_avoids(const uint32_t* const arr_orig,  const uint32_t* const avoids,  const unsigned arr_sz,  const unsigned avoids_sz,  const unsigned shiftby,  unsigned max_iterations);

int main(){
	const uint32_t arr1_sz = 10;
	uint32_t* const arr1 = reinterpret_cast<uint32_t*>(malloc(arr1_sz*sizeof(uint32_t)));
	const uint32_t arr2_sz = 100;
	uint32_t* const arr2 = reinterpret_cast<uint32_t*>(malloc(arr2_sz*sizeof(uint32_t)));
	
	const unsigned shiftby1 = 28;
	const unsigned shiftby2 = 25;
	const unsigned max_iterations = 100000;
	
	const uint32_t multiplier1 = finding_0xedc72f12_w_avoids(arr1, arr2, arr1_sz, arr2_sz, shiftby1, max_iterations);
	
	free(arr2);
	free(arr1);
	
	return 0;
}
