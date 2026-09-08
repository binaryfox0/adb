#include "spake2_u256.h"

#include "spake2_ctime.h"

#define SPAKE2__ARRSZ(arr) (sizeof((arr)) / sizeof((arr)[0]))

void spake2__u256_cmov(
        spake2__u256_t *out1,
        spake2__u256_t *arg1,
        const uint64_t mask)
{
    for(unsigned i = 0; i < SPAKE2__ARRSZ(out1->v); i++)
    {
        out1->v[i] = spake2__ctime_select_w(mask, 
                out1->v[i], arg1->v[i]);
    }
}

void spake2__u256_add(
        spake2__u256_t *out1, 
        const spake2__u256_t *arg1, 
        const spake2__u256_t *arg2)
{
    __uint128_t t;
    uint64_t carry;

    t = (__uint128_t)arg1->v[0] + arg2->v[0];
    out1->v[0] = (uint64_t)t;
    carry = (uint64_t)(t >> 64);

    t = (__uint128_t)arg1->v[1] + arg2->v[1] + carry;
    out1->v[1] = (uint64_t)t;
    carry = (uint64_t)(t >> 64);

    t = (__uint128_t)arg1->v[2] + arg2->v[2] + carry;
    out1->v[2] = (uint64_t)t;
    carry = (uint64_t)(t >> 64);

    t = (__uint128_t)arg1->v[3] + arg2->v[3] + carry;
    out1->v[3] = (uint64_t)t;
}
