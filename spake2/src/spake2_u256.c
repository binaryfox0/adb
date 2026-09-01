#include "spake2_u256.h"

void spake2__u256_0(
        spake2__u256_t u)
{
    u[0] = 0;
    u[1] = 0;
    u[2] = 0;
    u[3] = 0;
}

void spake2__u256_cmov(
        spake2__u256_t u,
        spake2__u256_t a,
        const uint64_t b)
{
    u[0] = (u[0] & ~b) | (a[0] & b);
    u[1] = (u[1] & ~b) | (a[1] & b);
    u[2] = (u[2] & ~b) | (a[2] & b);
    u[3] = (u[3] & ~b) | (a[3] & b);
}
void spake2__u256_add(
        spake2__u256_t u,
        const spake2__u256_t a,
        const spake2__u256_t b)
{
    uint64_t r0 = 0;
    uint64_t r1 = 0;
    uint64_t r2 = 0;
    uint64_t r3 = 0;
    uint64_t c0 = 0;
    uint64_t c1 = 0;
    uint64_t c2 = 0;

    r0 = a[0] + b[0];
    c0 = r0 < a[0];

    r1 = a[1] + b[1] + c0;
    c1 = (r1 < a[1]) || (c0 && r1 == a[1]);

    r2 = a[2] + b[2] + c1;
    c2 = (r2 < a[2]) || (c1 && r2 == a[2]);

    r3 = a[3] + b[3] + c2;

    u[0] = r0;
    u[1] = r1;
    u[2] = r2;
    u[3] = r3;
}

