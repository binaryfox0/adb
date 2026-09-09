#include "spake2_fe.h"

#include <string.h>
#include "spake2_ctime.h"

void spake2__fe_0(
        spake2__fe_t *out1) { 
    memset(out1, 0, sizeof(*out1)); 
}

void spake2__fe_1(
        spake2__fe_t *out1) { 
    memset(out1, 0, sizeof(*out1)); 
    out1->v[0] = 1;
}

void spake2__fe_copy(
        spake2__fe_t *out1, 
        const spake2__fe_t *arg1) { 
    memmove(out1, arg1, sizeof(*out1)); 
}

void spake2__fe_copy_lt(
        spake2__fe_loose_t *out1, 
        const spake2__fe_t *arg1) {
    memmove(out1, arg1, sizeof(*out1));
}

void spake2__fe_loose_0(
        spake2__fe_loose_t *out1) {
    memset(out1, 0, sizeof(*out1)); 
}

void spake2__fe_loose_1(
        spake2__fe_loose_t *out1) {
    memset(out1, 0, sizeof(*out1)); 
    out1->v[0] = 1;
}

// Replace (f,g) with (g,g) if b == 1;
// replace (f,g) with (f,g) if b == 0.
//
// Preconditions: b in {0,1}.
void spake2__fe_loose_cmov(
        spake2__fe_loose_t *out1, 
        const spake2__fe_loose_t *arg1, 
        const uint64_t b) 
{
    uint64_t inv_b = 0 - b;
    for(unsigned i = 0; i < SPAKE2__FE_LIMB_COUNT; i++) 
    {
        uint64_t x = out1->v[i] ^ arg1->v[i];
        x &= inv_b;
        out1->v[i] ^= x;
    }
}

// return 0 if f == 0
// return 1 if f != 0
int spake2__fe_loose_is_nonzero(
        const spake2__fe_loose_t *f) 
{
    spake2__fe_t tight;
    spake2__fe_carry(&tight, f);
    uint8_t s[32];
    spake2__fe_to_bytes(s, &tight);

    static const uint8_t zero[32] = {0};
    return memcmp(s, zero, sizeof(zero)) != 0;
}

// return 1 if f is in {1,3,5,...,q-2}
// return 0 if f is in {0,2,4,...,q-1}
int spake2__fe_is_negative(
        const spake2__fe_t *f) 
{
    uint8_t s[32];
    spake2__fe_to_bytes(s, f);
    return s[0] & 1;
}
/*
 * The function spake2__fe_neg negates a field element.
 *
 * Postconditions:
 *   eval out1 mod m = -eval arg1 mod m
 *
 */
void spake2__fe_neg(
        spake2__fe_loose_t *out1, 
        const spake2__fe_t *arg1) 
{
    uint64_t x1 = (UINT64_C(0xfffffffffffda) - (arg1->v[0]));
    uint64_t x2 = (UINT64_C(0xffffffffffffe) - (arg1->v[1]));
    uint64_t x3 = (UINT64_C(0xffffffffffffe) - (arg1->v[2]));
    uint64_t x4 = (UINT64_C(0xffffffffffffe) - (arg1->v[3]));
    uint64_t x5 = (UINT64_C(0xffffffffffffe) - (arg1->v[4]));
    out1->v[0] = x1;
    out1->v[1] = x2;
    out1->v[2] = x3;
    out1->v[3] = x4;
    out1->v[4] = x5;
}

/*
 * The function spake2__fe_add adds two field elements.
 *
 * Postconditions:
 *   eval out1 mod m = (eval arg1 + eval arg2) mod m
 *
 */
void spake2__fe_add(
        spake2__fe_loose_t *out1, 
        const spake2__fe_t *arg1, 
        const spake2__fe_t *arg2) 
{
    uint64_t x1 = ((arg1->v[0]) + (arg2->v[0]));
    uint64_t x2 = ((arg1->v[1]) + (arg2->v[1]));
    uint64_t x3 = ((arg1->v[2]) + (arg2->v[2]));
    uint64_t x4 = ((arg1->v[3]) + (arg2->v[3]));
    uint64_t x5 = ((arg1->v[4]) + (arg2->v[4]));
    out1->v[0] = x1;
    out1->v[1] = x2;
    out1->v[2] = x3;
    out1->v[3] = x4;
    out1->v[4] = x5;
}

/*
 * The function fiat_25519_sub subtracts two field elements.
 *
 * Postconditions:
 *   eval out1 mod m = (eval arg1 - eval arg2) mod m
 *
 */
void spake2__fe_sub(
        spake2__fe_loose_t *out1, 
        const spake2__fe_t *arg1, 
        const spake2__fe_t *arg2) 
{
    uint64_t x1 = ((UINT64_C(0xfffffffffffda) + (arg1->v[0])) - (arg2->v[0]));
    uint64_t x2 = ((UINT64_C(0xffffffffffffe) + (arg1->v[1])) - (arg2->v[1]));
    uint64_t x3 = ((UINT64_C(0xffffffffffffe) + (arg1->v[2])) - (arg2->v[2]));
    uint64_t x4 = ((UINT64_C(0xffffffffffffe) + (arg1->v[3])) - (arg2->v[3]));
    uint64_t x5 = ((UINT64_C(0xffffffffffffe) + (arg1->v[4])) - (arg2->v[4]));
    out1->v[0] = x1;
    out1->v[1] = x2;
    out1->v[2] = x3;
    out1->v[3] = x4;
    out1->v[4] = x5;
}

/*
 * The function carry_mul multiplies two field elements and reduces the result.
 *
 * Postconditions:
 *   eval out1 mod m = (eval arg1 * eval arg2) mod m
 *
 */
static void spake2__fe_carry_mul_impl(
        uint64_t out1[SPAKE2__FE_LIMB_COUNT], 
        const uint64_t arg1[SPAKE2__FE_LIMB_COUNT], 
        const uint64_t arg2[SPAKE2__FE_LIMB_COUNT]) 
{
    __uint128_t x1 = ((__uint128_t)(arg1[4]) * ((arg2[4]) * UINT8_C(0x13)));
    __uint128_t x2 = ((__uint128_t)(arg1[4]) * ((arg2[3]) * UINT8_C(0x13)));
    __uint128_t x3 = ((__uint128_t)(arg1[4]) * ((arg2[2]) * UINT8_C(0x13)));
    __uint128_t x4 = ((__uint128_t)(arg1[4]) * ((arg2[1]) * UINT8_C(0x13)));
    __uint128_t x5 = ((__uint128_t)(arg1[3]) * ((arg2[4]) * UINT8_C(0x13)));
    __uint128_t x6 = ((__uint128_t)(arg1[3]) * ((arg2[3]) * UINT8_C(0x13)));
    __uint128_t x7 = ((__uint128_t)(arg1[3]) * ((arg2[2]) * UINT8_C(0x13)));
    __uint128_t x8 = ((__uint128_t)(arg1[2]) * ((arg2[4]) * UINT8_C(0x13)));
    __uint128_t x9 = ((__uint128_t)(arg1[2]) * ((arg2[3]) * UINT8_C(0x13)));
    __uint128_t x10 = ((__uint128_t)(arg1[1]) * ((arg2[4]) * UINT8_C(0x13)));
    __uint128_t x11 = ((__uint128_t)(arg1[4]) * (arg2[0]));
    __uint128_t x12 = ((__uint128_t)(arg1[3]) * (arg2[1]));
    __uint128_t x13 = ((__uint128_t)(arg1[3]) * (arg2[0]));
    __uint128_t x14 = ((__uint128_t)(arg1[2]) * (arg2[2]));
    __uint128_t x15 = ((__uint128_t)(arg1[2]) * (arg2[1]));
    __uint128_t x16 = ((__uint128_t)(arg1[2]) * (arg2[0]));
    __uint128_t x17 = ((__uint128_t)(arg1[1]) * (arg2[3]));
    __uint128_t x18 = ((__uint128_t)(arg1[1]) * (arg2[2]));
    __uint128_t x19 = ((__uint128_t)(arg1[1]) * (arg2[1]));
    __uint128_t x20 = ((__uint128_t)(arg1[1]) * (arg2[0]));
    __uint128_t x21 = ((__uint128_t)(arg1[0]) * (arg2[4]));
    __uint128_t x22 = ((__uint128_t)(arg1[0]) * (arg2[3]));
    __uint128_t x23 = ((__uint128_t)(arg1[0]) * (arg2[2]));
    __uint128_t x24 = ((__uint128_t)(arg1[0]) * (arg2[1]));
    __uint128_t x25 = ((__uint128_t)(arg1[0]) * (arg2[0]));
    __uint128_t x26 = (x25 + (x10 + (x9 + (x7 + x4))));
    uint64_t x27 = (uint64_t)(x26 >> 51);
    uint64_t x28 = (uint64_t)(x26 & UINT64_C(0x7ffffffffffff));
    __uint128_t x29 = (x21 + (x17 + (x14 + (x12 + x11))));
    __uint128_t x30 = (x22 + (x18 + (x15 + (x13 + x1))));
    __uint128_t x31 = (x23 + (x19 + (x16 + (x5 + x2))));
    __uint128_t x32 = (x24 + (x20 + (x8 + (x6 + x3))));
    __uint128_t x33 = (x27 + x32);
    uint64_t x34 = (uint64_t)(x33 >> 51);
    uint64_t x35 = (uint64_t)(x33 & UINT64_C(0x7ffffffffffff));
    __uint128_t x36 = (x34 + x31);
    uint64_t x37 = (uint64_t)(x36 >> 51);
    uint64_t x38 = (uint64_t)(x36 & UINT64_C(0x7ffffffffffff));
    __uint128_t x39 = (x37 + x30);
    uint64_t x40 = (uint64_t)(x39 >> 51);
    uint64_t x41 = (uint64_t)(x39 & UINT64_C(0x7ffffffffffff));
    __uint128_t x42 = (x40 + x29);
    uint64_t x43 = (uint64_t)(x42 >> 51);
    uint64_t x44 = (uint64_t)(x42 & UINT64_C(0x7ffffffffffff));
    uint64_t x45 = (x43 * UINT8_C(0x13));
    uint64_t x46 = (x28 + x45);
    uint64_t x47 = (x46 >> 51);
    uint64_t x48 = (x46 & UINT64_C(0x7ffffffffffff));
    uint64_t x49 = (x47 + x35);
    uint8_t x50 = (uint8_t)(x49 >> 51);
    uint64_t x51 = (x49 & UINT64_C(0x7ffffffffffff));
    uint64_t x52 = (x50 + x38);
    
    out1[0] = x48;
    out1[1] = x51;
    out1[2] = x52;
    out1[3] = x41;
    out1[4] = x44;
}

void spake2__fe_mul_llt(
        spake2__fe_loose_t *out1, 
        const spake2__fe_loose_t *arg1, 
        const spake2__fe_t *arg2) {
    spake2__fe_carry_mul_impl(out1->v, arg1->v, arg2->v);
}

void spake2__fe_mul_ltt(
        spake2__fe_loose_t *out1,
        const spake2__fe_t *arg1, 
        const spake2__fe_t *arg2) {
    spake2__fe_carry_mul_impl(out1->v, arg1->v, arg2->v);
}

void spake2__fe_mul_ttt(
        spake2__fe_t *out1, 
        const spake2__fe_t *arg1, 
        const spake2__fe_t *arg2) {
    spake2__fe_carry_mul_impl(out1->v, arg1->v, arg2->v);
}

void spake2__fe_mul_ttl(
        spake2__fe_t *out1, 
        const spake2__fe_t *arg1, 
        const spake2__fe_loose_t *arg2) {
    spake2__fe_carry_mul_impl(out1->v, arg1->v, arg2->v);
}

void spake2__fe_mul_tlt(
        spake2__fe_t *out1, 
        const spake2__fe_loose_t *arg1, 
        const spake2__fe_t *arg2) {
    spake2__fe_carry_mul_impl(out1->v, arg1->v, arg2->v);
}

void spake2__fe_mul_tll(
        spake2__fe_t *out1, 
        const spake2__fe_loose_t *arg1, 
        const spake2__fe_loose_t *arg2) {
    spake2__fe_carry_mul_impl(out1->v, arg1->v, arg2->v);
}

/*
 * The function spake2__fe_carry reduces a field element.
 *
 * Postconditions:
 *   eval out1 mod m = eval arg1 mod m
 *
 */
void spake2__fe_carry(
        spake2__fe_t *out1, 
        const spake2__fe_loose_t *arg1) 
{
    uint64_t x1 = (arg1->v[0]);
    uint64_t x2 = ((x1 >> 51) + (arg1->v[1]));
    uint64_t x3 = ((x2 >> 51) + (arg1->v[2]));
    uint64_t x4 = ((x3 >> 51) + (arg1->v[3]));
    uint64_t x5 = ((x4 >> 51) + (arg1->v[4]));
    uint64_t x6 = ((x1 & UINT64_C(0x7ffffffffffff)) + ((x5 >> 51) * UINT8_C(0x13)));
    uint64_t x7 = ((uint8_t)(x6 >> 51) + (x2 & UINT64_C(0x7ffffffffffff)));
    uint64_t x8 = (x6 & UINT64_C(0x7ffffffffffff));
    uint64_t x9 = (x7 & UINT64_C(0x7ffffffffffff));
    uint64_t x10 = ((uint8_t)(x7 >> 51) + (x3 & UINT64_C(0x7ffffffffffff)));
    uint64_t x11 = (x4 & UINT64_C(0x7ffffffffffff));
    uint64_t x12 = (x5 & UINT64_C(0x7ffffffffffff));
    
    out1->v[0] = x8;
    out1->v[1] = x9;
    out1->v[2] = x10;
    out1->v[3] = x11;
    out1->v[4] = x12;
}

/*
 * The function fiat_25519_carry_square squares a field element and reduces the result.
 *
 * Postconditions:
 *   eval out1 mod m = (eval arg1 * eval arg1) mod m
 *
 */
static void spake2__fe_sq_impl(
        uint64_t out1[SPAKE2__FE_LIMB_COUNT], 
        const uint64_t arg1[SPAKE2__FE_LIMB_COUNT]) 
{
    uint64_t x1 = ((arg1[4]) * UINT8_C(0x13));
    uint64_t x2 = (x1 * 0x2);
    uint64_t x3 = ((arg1[4]) * 0x2);
    uint64_t x4 = ((arg1[3]) * UINT8_C(0x13));
    uint64_t x5 = (x4 * 0x2);
    uint64_t x6 = ((arg1[3]) * 0x2);
    uint64_t x7 = ((arg1[2]) * 0x2);
    uint64_t x8 = ((arg1[1]) * 0x2);
    __uint128_t x9 = ((__uint128_t)(arg1[4]) * x1);
    __uint128_t x10 = ((__uint128_t)(arg1[3]) * x2);
    __uint128_t x11 = ((__uint128_t)(arg1[3]) * x4);
    __uint128_t x12 = ((__uint128_t)(arg1[2]) * x2);
    __uint128_t x13 = ((__uint128_t)(arg1[2]) * x5);
    __uint128_t x14 = ((__uint128_t)(arg1[2]) * (arg1[2]));
    __uint128_t x15 = ((__uint128_t)(arg1[1]) * x2);
    __uint128_t x16 = ((__uint128_t)(arg1[1]) * x6);
    __uint128_t x17 = ((__uint128_t)(arg1[1]) * x7);
    __uint128_t x18 = ((__uint128_t)(arg1[1]) * (arg1[1]));
    __uint128_t x19 = ((__uint128_t)(arg1[0]) * x3);
    __uint128_t x20 = ((__uint128_t)(arg1[0]) * x6);
    __uint128_t x21 = ((__uint128_t)(arg1[0]) * x7);
    __uint128_t x22 = ((__uint128_t)(arg1[0]) * x8);
    __uint128_t x23 = ((__uint128_t)(arg1[0]) * (arg1[0]));
    __uint128_t x24 = (x23 + (x15 + x13));
    uint64_t x25 = (uint64_t)(x24 >> 51);
    uint64_t x26 = (uint64_t)(x24 & UINT64_C(0x7ffffffffffff));
    __uint128_t x27 = (x19 + (x16 + x14));
    __uint128_t x28 = (x20 + (x17 + x9));
    __uint128_t x29 = (x21 + (x18 + x10));
    __uint128_t x30 = (x22 + (x12 + x11));
    __uint128_t x31 = (x25 + x30);
    uint64_t x32 = (uint64_t)(x31 >> 51);
    uint64_t x33 = (uint64_t)(x31 & UINT64_C(0x7ffffffffffff));
    __uint128_t x34 = (x32 + x29);
    uint64_t x35 = (uint64_t)(x34 >> 51);
    uint64_t x36 = (uint64_t)(x34 & UINT64_C(0x7ffffffffffff));
    __uint128_t x37 = (x35 + x28);
    uint64_t x38 = (uint64_t)(x37 >> 51);
    uint64_t x39 = (uint64_t)(x37 & UINT64_C(0x7ffffffffffff));
    __uint128_t x40 = (x38 + x27);
    uint64_t x41 = (uint64_t)(x40 >> 51);
    uint64_t x42 = (uint64_t)(x40 & UINT64_C(0x7ffffffffffff));
    uint64_t x43 = (x41 * UINT8_C(0x13));
    uint64_t x44 = (x26 + x43);
    uint64_t x45 = (x44 >> 51);
    uint64_t x46 = (x44 & UINT64_C(0x7ffffffffffff));
    uint64_t x47 = (x45 + x33);
    uint8_t x48 = (uint8_t)(x47 >> 51);
    uint64_t x49 = (x47 & UINT64_C(0x7ffffffffffff));
    uint64_t x50 = (x48 + x36);
    
    out1[0] = x46;
    out1[1] = x49;
    out1[2] = x50;
    out1[3] = x39;
    out1[4] = x42;
}

void spake2__fe_sq_tt(
        spake2__fe_t *h, 
        const spake2__fe_t *f) {
    spake2__fe_sq_impl(h->v, f->v);
}

void spake2__fe_sq_tl(
        spake2__fe_t *h, 
        const spake2__fe_loose_t *f) {
    spake2__fe_sq_impl(h->v, f->v);
}

void spake2__fe_sq2_tt(
        spake2__fe_t *h, 
        const spake2__fe_t *f) 
{
    // h = f^2
    spake2__fe_sq_tt(h, f);

    // h = h + h
    spake2__fe_loose_t tmp;
    spake2__fe_add(&tmp, h, h);
    spake2__fe_carry(h, &tmp);
}

void spake2__fe_pow22523(
        spake2__fe_t *out, 
        const spake2__fe_t *z) 
{
    spake2__fe_t t0;
    spake2__fe_t t1;
    spake2__fe_t t2;

    spake2__fe_sq_tt(&t0, z);
    spake2__fe_sq_tt(&t1, &t0);
    spake2__fe_sq_tt(&t1, &t1);
    spake2__fe_mul_ttt(&t1, z, &t1);
    spake2__fe_mul_ttt(&t0, &t0, &t1);
    spake2__fe_sq_tt(&t0, &t0);
    spake2__fe_mul_ttt(&t0, &t1, &t0);
    spake2__fe_sq_tt(&t1, &t0);
    for(int i = 1; i < 5; i++) 
        spake2__fe_sq_tt(&t1, &t1);
    spake2__fe_mul_ttt(&t0, &t1, &t0);
    spake2__fe_sq_tt(&t1, &t0);
    for(int i = 1; i < 10; i++) 
        spake2__fe_sq_tt(&t1, &t1);
    spake2__fe_mul_ttt(&t1, &t1, &t0);
    spake2__fe_sq_tt(&t2, &t1);
    for(int i = 1; i < 20; i++) 
        spake2__fe_sq_tt(&t2, &t2);
    spake2__fe_mul_ttt(&t1, &t2, &t1);
    spake2__fe_sq_tt(&t1, &t1);
    for(int i = 1; i < 10; i++) 
        spake2__fe_sq_tt(&t1, &t1);
    spake2__fe_mul_ttt(&t0, &t1, &t0);
    spake2__fe_sq_tt(&t1, &t0);
    for(int i = 1; i < 50; i++) 
        spake2__fe_sq_tt(&t1, &t1);
    spake2__fe_mul_ttt(&t1, &t1, &t0);
    spake2__fe_sq_tt(&t2, &t1);
    for(int i = 1; i < 100; i++) 
        spake2__fe_sq_tt(&t2, &t2);
    spake2__fe_mul_ttt(&t1, &t2, &t1);
    spake2__fe_sq_tt(&t1, &t1);
    for(int i = 1; i < 50; i++) 
        spake2__fe_sq_tt(&t1, &t1);
    spake2__fe_mul_ttt(&t0, &t1, &t0);
    spake2__fe_sq_tt(&t0, &t0);
    spake2__fe_sq_tt(&t0, &t0);
    spake2__fe_mul_ttt(out, &t0, z);
}


static void spake2__fe_loose_invert(
        spake2__fe_t *out, 
        const spake2__fe_loose_t *z) 
{
    spake2__fe_t t0;
    spake2__fe_t t1;
    spake2__fe_t t2;
    spake2__fe_t t3;

    spake2__fe_sq_tl(&t0, z);
    spake2__fe_sq_tt(&t1, &t0);
    spake2__fe_sq_tt(&t1, &t1);
    spake2__fe_mul_tlt(&t1, z, &t1);
    spake2__fe_mul_ttt(&t0, &t0, &t1);
    spake2__fe_sq_tt(&t2, &t0);
    spake2__fe_mul_ttt(&t1, &t1, &t2);
    spake2__fe_sq_tt(&t2, &t1);
    for(int i = 1; i < 5; i++) 
        spake2__fe_sq_tt(&t2, &t2);
    spake2__fe_mul_ttt(&t1, &t2, &t1);
    spake2__fe_sq_tt(&t2, &t1);
    for(int i = 1; i < 10; i++) 
        spake2__fe_sq_tt(&t2, &t2);
    spake2__fe_mul_ttt(&t2, &t2, &t1);
    spake2__fe_sq_tt(&t3, &t2);
    for(int i = 1; i < 20; i++) 
        spake2__fe_sq_tt(&t3, &t3);
    spake2__fe_mul_ttt(&t2, &t3, &t2);
    spake2__fe_sq_tt(&t2, &t2);
    for(int i = 1; i < 10; i++) 
        spake2__fe_sq_tt(&t2, &t2);
    spake2__fe_mul_ttt(&t1, &t2, &t1);
    spake2__fe_sq_tt(&t2, &t1);
    for(int i = 1; i < 50; i++) 
        spake2__fe_sq_tt(&t2, &t2);
    spake2__fe_mul_ttt(&t2, &t2, &t1);
    spake2__fe_sq_tt(&t3, &t2);
    for(int i = 1; i < 100; i++) 
        spake2__fe_sq_tt(&t3, &t3);
    spake2__fe_mul_ttt(&t2, &t3, &t2);
    spake2__fe_sq_tt(&t2, &t2);
    for(int i = 1; i < 50; i++) 
        spake2__fe_sq_tt(&t2, &t2);
    spake2__fe_mul_ttt(&t1, &t2, &t1);
    spake2__fe_sq_tt(&t1, &t1);
    for(int i = 1; i < 5; i++) 
        spake2__fe_sq_tt(&t1, &t1);
    spake2__fe_mul_ttt(out, &t1, &t0);
}

void spake2__fe_invert(
        spake2__fe_t *out, 
        const spake2__fe_t *z) 
{
    spake2__fe_loose_t l;
    spake2__fe_copy_lt(&l, z);
    spake2__fe_loose_invert(out, &l);
}
/*
 * The function spake2__fe_from_bytes deserializes a field element from bytes in little-endian order.
 *
 * Postconditions:
 *   eval out1 mod m = bytes_eval arg1 mod m
 *
 * Input Bounds:
 *   arg1: [[0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0x7f]]
 */
void spake2__fe_from_bytes(
        spake2__fe_t *out1, 
        const uint8_t arg1[32]) 
{
    uint64_t x1 = ((uint64_t)(arg1[31]) << 44);
    uint64_t x2 = ((uint64_t)(arg1[30]) << 36);
    uint64_t x3 = ((uint64_t)(arg1[29]) << 28);
    uint64_t x4 = ((uint64_t)(arg1[28]) << 20);
    uint64_t x5 = ((uint64_t)(arg1[27]) << 12);
    uint64_t x6 = ((uint64_t)(arg1[26]) << 4);
    uint64_t x7 = ((uint64_t)(arg1[25]) << 47);
    uint64_t x8 = ((uint64_t)(arg1[24]) << 39);
    uint64_t x9 = ((uint64_t)(arg1[23]) << 31);
    uint64_t x10 = ((uint64_t)(arg1[22]) << 23);
    uint64_t x11 = ((uint64_t)(arg1[21]) << 15);
    uint64_t x12 = ((uint64_t)(arg1[20]) << 7);
    uint64_t x13 = ((uint64_t)(arg1[19]) << 50);
    uint64_t x14 = ((uint64_t)(arg1[18]) << 42);
    uint64_t x15 = ((uint64_t)(arg1[17]) << 34);
    uint64_t x16 = ((uint64_t)(arg1[16]) << 26);
    uint64_t x17 = ((uint64_t)(arg1[15]) << 18);
    uint64_t x18 = ((uint64_t)(arg1[14]) << 10);
    uint64_t x19 = ((uint64_t)(arg1[13]) << 2);
    uint64_t x20 = ((uint64_t)(arg1[12]) << 45);
    uint64_t x21 = ((uint64_t)(arg1[11]) << 37);
    uint64_t x22 = ((uint64_t)(arg1[10]) << 29);
    uint64_t x23 = ((uint64_t)(arg1[9]) << 21);
    uint64_t x24 = ((uint64_t)(arg1[8]) << 13);
    uint64_t x25 = ((uint64_t)(arg1[7]) << 5);
    uint64_t x26 = ((uint64_t)(arg1[6]) << 48);
    uint64_t x27 = ((uint64_t)(arg1[5]) << 40);
    uint64_t x28 = ((uint64_t)(arg1[4]) << 32);
    uint64_t x29 = ((uint64_t)(arg1[3]) << 24);
    uint64_t x30 = ((uint64_t)(arg1[2]) << 16);
    uint64_t x31 = ((uint64_t)(arg1[1]) << 8);
    uint8_t x32 = (arg1[0]);
    uint64_t x33 = (x31 + (uint64_t)x32);
    uint64_t x34 = (x30 + x33);
    uint64_t x35 = (x29 + x34);
    uint64_t x36 = (x28 + x35);
    uint64_t x37 = (x27 + x36);
    uint64_t x38 = (x26 + x37);
    uint64_t x39 = (x38 & UINT64_C(0x7ffffffffffff));
    uint8_t x40 = (uint8_t)(x38 >> 51);
    uint64_t x41 = (x25 + (uint64_t)x40);
    uint64_t x42 = (x24 + x41);
    uint64_t x43 = (x23 + x42);
    uint64_t x44 = (x22 + x43);
    uint64_t x45 = (x21 + x44);
    uint64_t x46 = (x20 + x45);
    uint64_t x47 = (x46 & UINT64_C(0x7ffffffffffff));
    uint8_t x48 = (uint8_t)(x46 >> 51);
    uint64_t x49 = (x19 + (uint64_t)x48);
    uint64_t x50 = (x18 + x49);
    uint64_t x51 = (x17 + x50);
    uint64_t x52 = (x16 + x51);
    uint64_t x53 = (x15 + x52);
    uint64_t x54 = (x14 + x53);
    uint64_t x55 = (x13 + x54);
    uint64_t x56 = (x55 & UINT64_C(0x7ffffffffffff));
    uint8_t x57 = (uint8_t)(x55 >> 51);
    uint64_t x58 = (x12 + (uint64_t)x57);
    uint64_t x59 = (x11 + x58);
    uint64_t x60 = (x10 + x59);
    uint64_t x61 = (x9 + x60);
    uint64_t x62 = (x8 + x61);
    uint64_t x63 = (x7 + x62);
    uint64_t x64 = (x63 & UINT64_C(0x7ffffffffffff));
    uint8_t x65 = (uint8_t)(x63 >> 51);
    uint64_t x66 = (x6 + (uint64_t)x65);
    uint64_t x67 = (x5 + x66);
    uint64_t x68 = (x4 + x67);
    uint64_t x69 = (x3 + x68);
    uint64_t x70 = (x2 + x69);
    uint64_t x71 = (x1 + x70);
    
    out1->v[0] = x39;
    out1->v[1] = x47;
    out1->v[2] = x56;
    out1->v[3] = x64;
    out1->v[4] = x71;
}

/*
 * The function fiat_25519_subborrowx_u51 is a subtraction with borrow.
 *
 * Postconditions:
 *   out1 = (-arg1 + arg2 + -arg3) mod 2^51
 *   out2 = -⌊(-arg1 + arg2 + -arg3) / 2^51⌋
 *
 * Input Bounds:
 *   arg1: [0x0 ~> 0x1]
 *   arg2: [0x0 ~> 0x7ffffffffffff]
 *   arg3: [0x0 ~> 0x7ffffffffffff]
 * Output Bounds:
 *   out1: [0x0 ~> 0x7ffffffffffff]
 *   out2: [0x0 ~> 0x1]
 */
static void spake2__subborrowx_u51(
        uint64_t* out1, 
        uint8_t* out2, 
        uint8_t arg1, 
        uint64_t arg2, 
        uint64_t arg3) 
{
    int64_t x1 = ((int64_t)(arg2 - (int64_t)arg1) - (int64_t)arg3);
    int8_t x2 = (int8_t)(x1 >> 51);
    uint64_t x3 = (x1 & UINT64_C(0x7ffffffffffff));
    
    *out1 = x3;
    *out2 = (uint8_t)(0x0 - x2);
}

/*
 * The function fiat_25519_cmovznz_u64 is a single-word conditional move.
 *
 * Postconditions:
 *   out1 = (if arg1 = 0 then arg2 else arg3)
 *
 * Input Bounds:
 *   arg1: [0x0 ~> 0x1]
 *   arg2: [0x0 ~> 0xffffffffffffffff]
 *   arg3: [0x0 ~> 0xffffffffffffffff]
 * Output Bounds:
 *   out1: [0x0 ~> 0xffffffffffffffff]
 */
static void spake2__cmovznz_u64(
        uint64_t* out1, 
        uint8_t arg1, 
        uint64_t arg2, 
        uint64_t arg3) 
{
    uint8_t x1 = (!(!arg1));
    uint64_t x2 = ((int8_t)(0x0 - x1) & UINT64_C(0xffffffffffffffff));
    uint64_t x3 = ((spake2__value_barrier_w(x2) & arg3) | 
            (spake2__value_barrier_w((~x2)) & arg2));
    
    *out1 = x3;
}

/*
 * The function fiat_25519_addcarryx_u51 is an addition with carry.
 *
 * Postconditions:
 *   out1 = (arg1 + arg2 + arg3) mod 2^51
 *   out2 = ⌊(arg1 + arg2 + arg3) / 2^51⌋
 *
 * Input Bounds:
 *   arg1: [0x0 ~> 0x1]
 *   arg2: [0x0 ~> 0x7ffffffffffff]
 *   arg3: [0x0 ~> 0x7ffffffffffff]
 * Output Bounds:
 *   out1: [0x0 ~> 0x7ffffffffffff]
 *   out2: [0x0 ~> 0x1]
 */
static void spake2__addcarryx_u51(
        uint64_t* out1, 
        uint8_t* out2, 
        uint8_t arg1, 
        uint64_t arg2, 
        uint64_t arg3) 
{
    uint64_t x1;
    uint64_t x2;
    uint8_t x3;
    x1 = ((arg1 + arg2) + arg3);
    x2 = (x1 & UINT64_C(0x7ffffffffffff));
    x3 = (uint8_t)(x1 >> 51);
    *out1 = x2;
    *out2 = x3;
}


/*
 * The function spake2__fe_to_bytes serializes a field element to bytes in little-endian order.
 *
 * Postconditions:
 *   out1 = map(λ x, ⌊((eval arg1 mod m) mod 2^(8 * (x + 1))) / 2^(8 * x)⌋) [0..31]
 *
 * Output Bounds:
 *   out1: [[0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0xff], [0x0 ~> 0x7f]]
 */
void spake2__fe_to_bytes(
        uint8_t out1[32], 
        const spake2__fe_t *arg1) 
{
  uint64_t x1 = 0, x3 = 0, x5 = 0;
  uint64_t x7 = 0, x9 = 0, x11 = 0;
  uint64_t x12 = 0, x14 = 0, x16 = 0;
  uint64_t x18 = 0, x20 = 0, x22 = 0;
  uint64_t x23 = 0, x24 = 0, x25 = 0;
  uint64_t x27 = 0, x29 = 0, x31 = 0;
  uint64_t x33 = 0, x35 = 0, x38 = 0;
  uint64_t x40 = 0, x42 = 0, x44 = 0;
  uint64_t x46 = 0, x48 = 0, x51 = 0;
  uint64_t x53 = 0, x55 = 0, x57 = 0;
  uint64_t x59 = 0, x61 = 0, x63 = 0;
  uint64_t x66 = 0, x68 = 0, x70 = 0;
  uint64_t x72 = 0, x74 = 0, x76 = 0;
  uint64_t x79 = 0, x81 = 0, x83 = 0;
  uint64_t x85 = 0, x87 = 0, x89 = 0;

  uint8_t x2 = 0, x4 = 0, x6 = 0;
  uint8_t x8 = 0, x10 = 0, x13 = 0;
  uint8_t x15 = 0, x17 = 0, x19 = 0;
  uint8_t x21 = 0, x26 = 0, x28 = 0;
  uint8_t x30 = 0, x32 = 0, x34 = 0;
  uint8_t x36 = 0, x37 = 0, x39 = 0;
  uint8_t x41 = 0, x43 = 0, x45 = 0;
  uint8_t x47 = 0, x49 = 0, x50 = 0;
  uint8_t x52 = 0, x54 = 0, x56 = 0;
  uint8_t x58 = 0, x60 = 0, x62 = 0;
  uint8_t x64 = 0, x65 = 0, x67 = 0;
  uint8_t x69 = 0, x71 = 0, x73 = 0;
  uint8_t x75 = 0, x77 = 0, x78 = 0;
  uint8_t x80 = 0, x82 = 0, x84 = 0;
  uint8_t x86 = 0, x88 = 0, x90 = 0;
  uint8_t x91 = 0;

  spake2__subborrowx_u51(&x1, &x2, 
          0x0, (arg1->v[0]), UINT64_C(0x7ffffffffffed));
  spake2__subborrowx_u51(&x3, &x4, 
          x2, (arg1->v[1]), UINT64_C(0x7ffffffffffff));
  spake2__subborrowx_u51(&x5, &x6, 
          x4, (arg1->v[2]), UINT64_C(0x7ffffffffffff));
  spake2__subborrowx_u51(&x7, &x8, 
          x6, (arg1->v[3]), UINT64_C(0x7ffffffffffff));
  spake2__subborrowx_u51(&x9, &x10, 
          x8, (arg1->v[4]), UINT64_C(0x7ffffffffffff));

  spake2__cmovznz_u64(&x11, x10, 0x0, UINT64_C(0xffffffffffffffff));

  spake2__addcarryx_u51(&x12, &x13, 
          0x0, x1, (x11 & UINT64_C(0x7ffffffffffed)));
  spake2__addcarryx_u51(&x14, &x15, 
          x13, x3, (x11 & UINT64_C(0x7ffffffffffff)));
  spake2__addcarryx_u51(&x16, &x17, 
          x15, x5, (x11 & UINT64_C(0x7ffffffffffff)));
  spake2__addcarryx_u51(&x18, &x19, 
          x17, x7, (x11 & UINT64_C(0x7ffffffffffff)));
  spake2__addcarryx_u51(&x20, &x21, 
          x19, x9, (x11 & UINT64_C(0x7ffffffffffff)));
  x22 = (x20 << 4);
  x23 = (x18 * (uint64_t)0x2);
  x24 = (x16 << 6);
  x25 = (x14 << 3);
  x26 = (uint8_t)(x12 & UINT8_C(0xff));
  x27 = (x12 >> 8);
  x28 = (uint8_t)(x27 & UINT8_C(0xff));
  x29 = (x27 >> 8);
  x30 = (uint8_t)(x29 & UINT8_C(0xff));
  x31 = (x29 >> 8);
  x32 = (uint8_t)(x31 & UINT8_C(0xff));
  x33 = (x31 >> 8);
  x34 = (uint8_t)(x33 & UINT8_C(0xff));
  x35 = (x33 >> 8);
  x36 = (uint8_t)(x35 & UINT8_C(0xff));
  x37 = (uint8_t)(x35 >> 8);
  x38 = (x25 + (uint64_t)x37);
  x39 = (uint8_t)(x38 & UINT8_C(0xff));
  x40 = (x38 >> 8);
  x41 = (uint8_t)(x40 & UINT8_C(0xff));
  x42 = (x40 >> 8);
  x43 = (uint8_t)(x42 & UINT8_C(0xff));
  x44 = (x42 >> 8);
  x45 = (uint8_t)(x44 & UINT8_C(0xff));
  x46 = (x44 >> 8);
  x47 = (uint8_t)(x46 & UINT8_C(0xff));
  x48 = (x46 >> 8);
  x49 = (uint8_t)(x48 & UINT8_C(0xff));
  x50 = (uint8_t)(x48 >> 8);
  x51 = (x24 + (uint64_t)x50);
  x52 = (uint8_t)(x51 & UINT8_C(0xff));
  x53 = (x51 >> 8);
  x54 = (uint8_t)(x53 & UINT8_C(0xff));
  x55 = (x53 >> 8);
  x56 = (uint8_t)(x55 & UINT8_C(0xff));
  x57 = (x55 >> 8);
  x58 = (uint8_t)(x57 & UINT8_C(0xff));
  x59 = (x57 >> 8);
  x60 = (uint8_t)(x59 & UINT8_C(0xff));
  x61 = (x59 >> 8);
  x62 = (uint8_t)(x61 & UINT8_C(0xff));
  x63 = (x61 >> 8);
  x64 = (uint8_t)(x63 & UINT8_C(0xff));
  x65 = (uint8_t)(x63 >> 8);
  x66 = (x23 + (uint64_t)x65);
  x67 = (uint8_t)(x66 & UINT8_C(0xff));
  x68 = (x66 >> 8);
  x69 = (uint8_t)(x68 & UINT8_C(0xff));
  x70 = (x68 >> 8);
  x71 = (uint8_t)(x70 & UINT8_C(0xff));
  x72 = (x70 >> 8);
  x73 = (uint8_t)(x72 & UINT8_C(0xff));
  x74 = (x72 >> 8);
  x75 = (uint8_t)(x74 & UINT8_C(0xff));
  x76 = (x74 >> 8);
  x77 = (uint8_t)(x76 & UINT8_C(0xff));
  x78 = (uint8_t)(x76 >> 8);
  x79 = (x22 + (uint64_t)x78);
  x80 = (uint8_t)(x79 & UINT8_C(0xff));
  x81 = (x79 >> 8);
  x82 = (uint8_t)(x81 & UINT8_C(0xff));
  x83 = (x81 >> 8);
  x84 = (uint8_t)(x83 & UINT8_C(0xff));
  x85 = (x83 >> 8);
  x86 = (uint8_t)(x85 & UINT8_C(0xff));
  x87 = (x85 >> 8);
  x88 = (uint8_t)(x87 & UINT8_C(0xff));
  x89 = (x87 >> 8);
  x90 = (uint8_t)(x89 & UINT8_C(0xff));
  x91 = (uint8_t)(x89 >> 8);

  out1[0] = x26;
  out1[1] = x28;
  out1[2] = x30;
  out1[3] = x32;
  out1[4] = x34;
  out1[5] = x36;
  out1[6] = x39;
  out1[7] = x41;
  out1[8] = x43;
  out1[9] = x45;
  out1[10] = x47;
  out1[11] = x49;
  out1[12] = x52;
  out1[13] = x54;
  out1[14] = x56;
  out1[15] = x58;
  out1[16] = x60;
  out1[17] = x62;
  out1[18] = x64;
  out1[19] = x67;
  out1[20] = x69;
  out1[21] = x71;
  out1[22] = x73;
  out1[23] = x75;
  out1[24] = x77;
  out1[25] = x80;
  out1[26] = x82;
  out1[27] = x84;
  out1[28] = x86;
  out1[29] = x88;
  out1[30] = x90;
  out1[31] = x91;
}
