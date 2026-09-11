#pragma once
#include <stdint.h>

// 2^(k/12) for k=0..12 in 16.16 fixed point (generated)
static const uint32_t SEMITONE_RATIO_Q16[13] = {
  65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193, 104032, 110218, 116772, 123715, 131072
};
