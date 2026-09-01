#ifndef SPAKE2_U256_H
#define SPAKE2_U256_H

#include <stdint.h>

typedef uint64_t spake2__u256_t[4];

void spake2__u256_0(
        spake2__u256_t u);

void spake2__u256_cmov(
        spake2__u256_t u,
        spake2__u256_t a,
        const uint64_t b);;

void spake2__u256_add(
        spake2__u256_t u, 
        const spake2__u256_t a, 
        const spake2__u256_t b);

#endif
