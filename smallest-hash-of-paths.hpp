#pragma once

#include <cstdint>

extern "C"
uint32_t finding_0xedc72f12(const uint32_t* const arr_orig,  const unsigned arr_sz,  const unsigned shiftby,  unsigned max_iterations);

extern "C"
uint32_t finding_0xedc72f12_w_avoids(const uint32_t* const arr_orig,  const uint32_t* const avoids,  const unsigned arr_sz,  const unsigned avoids_sz,  const unsigned shiftby,  unsigned max_iterations);

uint64_t xorshift128plus();

uint32_t find_max(const uint32_t* const arr,  const uint32_t arr_sz);

uint32_t get_shiftby(const uint32_t outputs_sz);
