#ifndef SPAKE2_CONSTT_H
#define SPAKE2_CONSTT_H

#include <stdint.h>

static inline uint64_t spake2__constant_time_msb_w(uint64_t a) {
  return 0u - (a >> (sizeof(a) * 8 - 1));
}

static inline uint64_t spake2__constant_time_lt_w(uint64_t a, uint64_t b) {
    return spake2__constant_time_msb_w(a ^ ((a ^ b) | ((a - b) ^ a)));
}

static inline uint8_t spake2__constant_time_lt_8(uint64_t a, uint64_t b) {
  return (uint8_t)(spake2__constant_time_lt_w(a, b));
}

static inline uint64_t spake2__constant_time_ge_w(uint64_t a, uint64_t b) {
  return ~spake2__constant_time_lt_w(a, b);
}

static inline uint8_t spake2__constant_time_ge_8(uint64_t a, uint64_t b) {
  return (uint8_t)(spake2__constant_time_ge_w(a, b));
}

static inline uint64_t spake2__constant_time_is_zero_w(uint64_t a) {
    return spake2__constant_time_msb_w(~a & (a - 1));
}

static inline uint8_t spake2__constant_time_is_zero_8(uint64_t a) {
  return (uint8_t)(spake2__constant_time_is_zero_w(a));
}

static inline uint64_t spake2__constant_time_eq_w(uint64_t a, uint64_t b) {
  return spake2__constant_time_is_zero_w(a ^ b);
}

#endif
