#ifndef SPAKE2_U256_H
#define SPAKE2_U256_H

#include <stdint.h>

typedef struct {
    uint64_t v[4];
} spake2__u256_t;

void spake2__u256_cmov(
        spake2__u256_t *out1,
        spake2__u256_t *arg1,
        const uint64_t mask);

void spake2__u256_add(
        spake2__u256_t *out1, 
        const spake2__u256_t *arg1, 
        const spake2__u256_t *arg2);
#endif
