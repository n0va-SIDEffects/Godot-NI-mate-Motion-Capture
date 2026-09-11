#pragma once
/*
 * IMA ADPCM decoder for a continuous 4-bit stream without block headers
 * (low nibble first). The matching encoder lives in tools/import_sample.py.
 * Header-only so the host-side test can compile it without the Pebble SDK.
 */
#include <stdint.h>

typedef struct { int32_t predictor; int32_t index; } ImaState;

static const int8_t IMA_INDEX_TABLE[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
static const int16_t IMA_STEP_TABLE[89] = {
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
  73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
  494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
  2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
  11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static inline int16_t ima_decode(ImaState *st, uint8_t nibble) {
  int32_t step = IMA_STEP_TABLE[st->index];
  int32_t diff = step >> 3;
  if (nibble & 1) diff += step >> 2;
  if (nibble & 2) diff += step >> 1;
  if (nibble & 4) diff += step;
  if (nibble & 8) diff = -diff;
  int32_t pred = st->predictor + diff;
  if (pred > 32767) pred = 32767;
  if (pred < -32768) pred = -32768;
  st->predictor = pred;
  st->index += IMA_INDEX_TABLE[nibble];
  if (st->index < 0) st->index = 0;
  if (st->index > 88) st->index = 88;
  return (int16_t)pred;
}
