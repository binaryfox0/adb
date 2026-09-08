#include "spake2_fe.h"

#include <string.h>

void spake2__fe_0(
        spake2__fe_t *out1) { 
    memset(out1, 0, sizeof(*out1)); 
}

void spake2__fe_1(
        spake2__fe_t *out1) { 
    memset(out1, 0, sizeof(*out1)); 
    out1->v[0] = 1;
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
        x &= b;
        out1->v[i] ^= x;
    }
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
    uint64_t x1; 
    uint64_t x2;
    uint64_t x3;
    uint64_t x4;
    uint64_t x5;
    x1 = ((arg1->v[0]) + (arg2->v[0]));
    x2 = ((arg1->v[1]) + (arg2->v[1]));
    x3 = ((arg1->v[2]) + (arg2->v[2]));
    x4 = ((arg1->v[3]) + (arg2->v[3]));
    x5 = ((arg1->v[4]) + (arg2->v[4]));
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
    uint64_t x1;
    uint64_t x2;
    uint64_t x3;
    uint64_t x4;
    uint64_t x5;
    x1 = ((UINT64_C(0xfffffffffffda) + (arg1->v[0])) - (arg2->v[0]));
    x2 = ((UINT64_C(0xffffffffffffe) + (arg1->v[1])) - (arg2->v[1]));
    x3 = ((UINT64_C(0xffffffffffffe) + (arg1->v[2])) - (arg2->v[2]));
    x4 = ((UINT64_C(0xffffffffffffe) + (arg1->v[3])) - (arg2->v[3]));
    x5 = ((UINT64_C(0xffffffffffffe) + (arg1->v[4])) - (arg2->v[4]));
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
    __uint128_t x1;
    __uint128_t x2;
    __uint128_t x3;
    __uint128_t x4;
    __uint128_t x5;
    __uint128_t x6;
    __uint128_t x7;
    __uint128_t x8;
    __uint128_t x9;
    __uint128_t x10;
    __uint128_t x11;
    __uint128_t x12;
    __uint128_t x13;
    __uint128_t x14;
    __uint128_t x15;
    __uint128_t x16;
    __uint128_t x17;
    __uint128_t x18;
    __uint128_t x19;
    __uint128_t x20;
    __uint128_t x21;
    __uint128_t x22;
    __uint128_t x23;
    __uint128_t x24;
    __uint128_t x25;
    __uint128_t x26;
    uint64_t x27;
    uint64_t x28;
    __uint128_t x29;
    __uint128_t x30;
    __uint128_t x31;
    __uint128_t x32;
    __uint128_t x33;
    uint64_t x34;
    uint64_t x35;
    __uint128_t x36;
    uint64_t x37;
    uint64_t x38;
    __uint128_t x39;
    uint64_t x40;
    uint64_t x41;
    __uint128_t x42;
    uint64_t x43;
    uint64_t x44;
    uint64_t x45;
    uint64_t x46;
    uint64_t x47;
    uint64_t x48;
    uint64_t x49;
    uint8_t x50;
    uint64_t x51;
    uint64_t x52;
    x1 = ((__uint128_t)(arg1[4]) * ((arg2[4]) * UINT8_C(0x13)));
    x2 = ((__uint128_t)(arg1[4]) * ((arg2[3]) * UINT8_C(0x13)));
    x3 = ((__uint128_t)(arg1[4]) * ((arg2[2]) * UINT8_C(0x13)));
    x4 = ((__uint128_t)(arg1[4]) * ((arg2[1]) * UINT8_C(0x13)));
    x5 = ((__uint128_t)(arg1[3]) * ((arg2[4]) * UINT8_C(0x13)));
    x6 = ((__uint128_t)(arg1[3]) * ((arg2[3]) * UINT8_C(0x13)));
    x7 = ((__uint128_t)(arg1[3]) * ((arg2[2]) * UINT8_C(0x13)));
    x8 = ((__uint128_t)(arg1[2]) * ((arg2[4]) * UINT8_C(0x13)));
    x9 = ((__uint128_t)(arg1[2]) * ((arg2[3]) * UINT8_C(0x13)));
    x10 = ((__uint128_t)(arg1[1]) * ((arg2[4]) * UINT8_C(0x13)));
    x11 = ((__uint128_t)(arg1[4]) * (arg2[0]));
    x12 = ((__uint128_t)(arg1[3]) * (arg2[1]));
    x13 = ((__uint128_t)(arg1[3]) * (arg2[0]));
    x14 = ((__uint128_t)(arg1[2]) * (arg2[2]));
    x15 = ((__uint128_t)(arg1[2]) * (arg2[1]));
    x16 = ((__uint128_t)(arg1[2]) * (arg2[0]));
    x17 = ((__uint128_t)(arg1[1]) * (arg2[3]));
    x18 = ((__uint128_t)(arg1[1]) * (arg2[2]));
    x19 = ((__uint128_t)(arg1[1]) * (arg2[1]));
    x20 = ((__uint128_t)(arg1[1]) * (arg2[0]));
    x21 = ((__uint128_t)(arg1[0]) * (arg2[4]));
    x22 = ((__uint128_t)(arg1[0]) * (arg2[3]));
    x23 = ((__uint128_t)(arg1[0]) * (arg2[2]));
    x24 = ((__uint128_t)(arg1[0]) * (arg2[1]));
    x25 = ((__uint128_t)(arg1[0]) * (arg2[0]));
    x26 = (x25 + (x10 + (x9 + (x7 + x4))));
    x27 = (uint64_t)(x26 >> 51);
    x28 = (uint64_t)(x26 & UINT64_C(0x7ffffffffffff));
    x29 = (x21 + (x17 + (x14 + (x12 + x11))));
    x30 = (x22 + (x18 + (x15 + (x13 + x1))));
    x31 = (x23 + (x19 + (x16 + (x5 + x2))));
    x32 = (x24 + (x20 + (x8 + (x6 + x3))));
    x33 = (x27 + x32);
    x34 = (uint64_t)(x33 >> 51);
    x35 = (uint64_t)(x33 & UINT64_C(0x7ffffffffffff));
    x36 = (x34 + x31);
    x37 = (uint64_t)(x36 >> 51);
    x38 = (uint64_t)(x36 & UINT64_C(0x7ffffffffffff));
    x39 = (x37 + x30);
    x40 = (uint64_t)(x39 >> 51);
    x41 = (uint64_t)(x39 & UINT64_C(0x7ffffffffffff));
    x42 = (x40 + x29);
    x43 = (uint64_t)(x42 >> 51);
    x44 = (uint64_t)(x42 & UINT64_C(0x7ffffffffffff));
    x45 = (x43 * UINT8_C(0x13));
    x46 = (x28 + x45);
    x47 = (x46 >> 51);
    x48 = (x46 & UINT64_C(0x7ffffffffffff));
    x49 = (x47 + x35);
    x50 = (uint8_t)(x49 >> 51);
    x51 = (x49 & UINT64_C(0x7ffffffffffff));
    x52 = (x50 + x38);
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
  uint64_t x1;
  uint64_t x2;
  uint64_t x3;
  uint64_t x4;
  uint64_t x5;
  uint64_t x6;
  uint64_t x7;
  uint64_t x8;
  uint64_t x9;
  uint64_t x10;
  uint64_t x11;
  uint64_t x12;
  x1 = (arg1->v[0]);
  x2 = ((x1 >> 51) + (arg1->v[1]));
  x3 = ((x2 >> 51) + (arg1->v[2]));
  x4 = ((x3 >> 51) + (arg1->v[3]));
  x5 = ((x4 >> 51) + (arg1->v[4]));
  x6 = ((x1 & UINT64_C(0x7ffffffffffff)) + ((x5 >> 51) * UINT8_C(0x13)));
  x7 = ((uint8_t)(x6 >> 51) + (x2 & UINT64_C(0x7ffffffffffff)));
  x8 = (x6 & UINT64_C(0x7ffffffffffff));
  x9 = (x7 & UINT64_C(0x7ffffffffffff));
  x10 = ((uint8_t)(x7 >> 51) + (x3 & UINT64_C(0x7ffffffffffff)));
  x11 = (x4 & UINT64_C(0x7ffffffffffff));
  x12 = (x5 & UINT64_C(0x7ffffffffffff));
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
    uint64_t x1;
    uint64_t x2;
    uint64_t x3;
    uint64_t x4;
    uint64_t x5;
    uint64_t x6;
    uint64_t x7;
    uint64_t x8;
    __uint128_t x9;
    __uint128_t x10;
    __uint128_t x11;
    __uint128_t x12;
    __uint128_t x13;
    __uint128_t x14;
    __uint128_t x15;
    __uint128_t x16;
    __uint128_t x17;
    __uint128_t x18;
    __uint128_t x19;
    __uint128_t x20;
    __uint128_t x21;
    __uint128_t x22;
    __uint128_t x23;
    __uint128_t x24;
    uint64_t x25;
    uint64_t x26;
    __uint128_t x27;
    __uint128_t x28;
    __uint128_t x29;
    __uint128_t x30;
    __uint128_t x31;
    uint64_t x32;
    uint64_t x33;
    __uint128_t x34;
    uint64_t x35;
    uint64_t x36;
    __uint128_t x37;
    uint64_t x38;
    uint64_t x39;
    __uint128_t x40;
    uint64_t x41;
    uint64_t x42;
    uint64_t x43;
    uint64_t x44;
    uint64_t x45;
    uint64_t x46;
    uint64_t x47;
    uint8_t x48;
    uint64_t x49;
    uint64_t x50;
    x1 = ((arg1[4]) * UINT8_C(0x13));
    x2 = (x1 * 0x2);
    x3 = ((arg1[4]) * 0x2);
    x4 = ((arg1[3]) * UINT8_C(0x13));
    x5 = (x4 * 0x2);
    x6 = ((arg1[3]) * 0x2);
    x7 = ((arg1[2]) * 0x2);
    x8 = ((arg1[1]) * 0x2);
    x9 = ((__uint128_t)(arg1[4]) * x1);
    x10 = ((__uint128_t)(arg1[3]) * x2);
    x11 = ((__uint128_t)(arg1[3]) * x4);
    x12 = ((__uint128_t)(arg1[2]) * x2);
    x13 = ((__uint128_t)(arg1[2]) * x5);
    x14 = ((__uint128_t)(arg1[2]) * (arg1[2]));
    x15 = ((__uint128_t)(arg1[1]) * x2);
    x16 = ((__uint128_t)(arg1[1]) * x6);
    x17 = ((__uint128_t)(arg1[1]) * x7);
    x18 = ((__uint128_t)(arg1[1]) * (arg1[1]));
    x19 = ((__uint128_t)(arg1[0]) * x3);
    x20 = ((__uint128_t)(arg1[0]) * x6);
    x21 = ((__uint128_t)(arg1[0]) * x7);
    x22 = ((__uint128_t)(arg1[0]) * x8);
    x23 = ((__uint128_t)(arg1[0]) * (arg1[0]));
    x24 = (x23 + (x15 + x13));
    x25 = (uint64_t)(x24 >> 51);
    x26 = (uint64_t)(x24 & UINT64_C(0x7ffffffffffff));
    x27 = (x19 + (x16 + x14));
    x28 = (x20 + (x17 + x9));
    x29 = (x21 + (x18 + x10));
    x30 = (x22 + (x12 + x11));
    x31 = (x25 + x30);
    x32 = (uint64_t)(x31 >> 51);
    x33 = (uint64_t)(x31 & UINT64_C(0x7ffffffffffff));
    x34 = (x32 + x29);
    x35 = (uint64_t)(x34 >> 51);
    x36 = (uint64_t)(x34 & UINT64_C(0x7ffffffffffff));
    x37 = (x35 + x28);
    x38 = (uint64_t)(x37 >> 51);
    x39 = (uint64_t)(x37 & UINT64_C(0x7ffffffffffff));
    x40 = (x38 + x27);
    x41 = (uint64_t)(x40 >> 51);
    x42 = (uint64_t)(x40 & UINT64_C(0x7ffffffffffff));
    x43 = (x41 * UINT8_C(0x13));
    x44 = (x26 + x43);
    x45 = (x44 >> 51);
    x46 = (x44 & UINT64_C(0x7ffffffffffff));
    x47 = (x45 + x33);
    x48 = (uint8_t)(x47 >> 51);
    x49 = (x47 & UINT64_C(0x7ffffffffffff));
    x50 = (x48 + x36);
    out1[0] = x46;
    out1[1] = x49;
    out1[2] = x50;
    out1[3] = x39;
    out1[4] = x42;
}

static void spake2__fe_sq_tt(
        spake2__fe_t *h, 
        const spake2__fe_t *f) {
    spake2__fe_sq_impl(h->v, f->v);
}

static void spake2__fe_sq_tl(
        spake2__fe_t *h, 
        const spake2__fe_loose_t *f) {
    spake2__fe_sq_impl(h->v, f->v);
}


static void spake2__fe_loose_invert(
        spake2__fe_t *out, 
        const spake2__fe_loose_t *z) {
  spake2__fe_t t0;
  spake2__fe_t t1;
  spake2__fe_t t2;
  spake2__fe_t t3;
  int i;

  spake2__fe_sq_tl(&t0, z);
  spake2__fe_sq_tt(&t1, &t0);
  for (i = 1; i < 2; ++i) {
    spake2__fe_sq_tt(&t1, &t1);
  }
  spake2__fe_mul_tlt(&t1, z, &t1);
  spake2__fe_mul_ttt(&t0, &t0, &t1);
  spake2__fe_sq_tt(&t2, &t0);
  spake2__fe_mul_ttt(&t1, &t1, &t2);
  spake2__fe_sq_tt(&t2, &t1);
  for (i = 1; i < 5; ++i) {
    spake2__fe_sq_tt(&t2, &t2);
  }
  spake2__fe_mul_ttt(&t1, &t2, &t1);
  spake2__fe_sq_tt(&t2, &t1);
  for (i = 1; i < 10; ++i) {
    spake2__fe_sq_tt(&t2, &t2);
  }
  spake2__fe_mul_ttt(&t2, &t2, &t1);
  spake2__fe_sq_tt(&t3, &t2);
  for (i = 1; i < 20; ++i) {
    spake2__fe_sq_tt(&t3, &t3);
  }
  spake2__fe_mul_ttt(&t2, &t3, &t2);
  spake2__fe_sq_tt(&t2, &t2);
  for (i = 1; i < 10; ++i) {
    spake2__fe_sq_tt(&t2, &t2);
  }
  spake2__fe_mul_ttt(&t1, &t2, &t1);
  spake2__fe_sq_tt(&t2, &t1);
  for (i = 1; i < 50; ++i) {
    spake2__fe_sq_tt(&t2, &t2);
  }
  spake2__fe_mul_ttt(&t2, &t2, &t1);
  spake2__fe_sq_tt(&t3, &t2);
  for (i = 1; i < 100; ++i) {
    spake2__fe_sq_tt(&t3, &t3);
  }
  spake2__fe_mul_ttt(&t2, &t3, &t2);
  spake2__fe_sq_tt(&t2, &t2);
  for (i = 1; i < 50; ++i) {
    spake2__fe_sq_tt(&t2, &t2);
  }
  spake2__fe_mul_ttt(&t1, &t2, &t1);
  spake2__fe_sq_tt(&t1, &t1);
  for (i = 1; i < 5; ++i) {
    spake2__fe_sq_tt(&t1, &t1);
  }
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
    uint64_t x1;
    uint64_t x2;
    uint64_t x3;
    uint64_t x4;
    uint64_t x5;
    uint64_t x6;
    uint64_t x7;
    uint64_t x8;
    uint64_t x9;
    uint64_t x10;
    uint64_t x11;
    uint64_t x12;
    uint64_t x13;
    uint64_t x14;
    uint64_t x15;
    uint64_t x16;
    uint64_t x17;
    uint64_t x18;
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29;
    uint64_t x30;
    uint64_t x31;
    uint8_t x32;
    uint64_t x33;
    uint64_t x34;
    uint64_t x35;
    uint64_t x36;
    uint64_t x37;
    uint64_t x38;
    uint64_t x39;
    uint8_t x40;
    uint64_t x41;
    uint64_t x42;
    uint64_t x43;
    uint64_t x44;
    uint64_t x45;
    uint64_t x46;
    uint64_t x47;
    uint8_t x48;
    uint64_t x49;
    uint64_t x50;
    uint64_t x51;
    uint64_t x52;
    uint64_t x53;
    uint64_t x54;
    uint64_t x55;
    uint64_t x56;
    uint8_t x57;
    uint64_t x58;
    uint64_t x59;
    uint64_t x60;
    uint64_t x61;
    uint64_t x62;
    uint64_t x63;
    uint64_t x64;
    uint8_t x65;
    uint64_t x66;
    uint64_t x67;
    uint64_t x68;
    uint64_t x69;
    uint64_t x70;
    uint64_t x71;
    x1 = ((uint64_t)(arg1[31]) << 44);
    x2 = ((uint64_t)(arg1[30]) << 36);
    x3 = ((uint64_t)(arg1[29]) << 28);
    x4 = ((uint64_t)(arg1[28]) << 20);
    x5 = ((uint64_t)(arg1[27]) << 12);
    x6 = ((uint64_t)(arg1[26]) << 4);
    x7 = ((uint64_t)(arg1[25]) << 47);
    x8 = ((uint64_t)(arg1[24]) << 39);
    x9 = ((uint64_t)(arg1[23]) << 31);
    x10 = ((uint64_t)(arg1[22]) << 23);
    x11 = ((uint64_t)(arg1[21]) << 15);
    x12 = ((uint64_t)(arg1[20]) << 7);
    x13 = ((uint64_t)(arg1[19]) << 50);
    x14 = ((uint64_t)(arg1[18]) << 42);
    x15 = ((uint64_t)(arg1[17]) << 34);
    x16 = ((uint64_t)(arg1[16]) << 26);
    x17 = ((uint64_t)(arg1[15]) << 18);
    x18 = ((uint64_t)(arg1[14]) << 10);
    x19 = ((uint64_t)(arg1[13]) << 2);
    x20 = ((uint64_t)(arg1[12]) << 45);
    x21 = ((uint64_t)(arg1[11]) << 37);
    x22 = ((uint64_t)(arg1[10]) << 29);
    x23 = ((uint64_t)(arg1[9]) << 21);
    x24 = ((uint64_t)(arg1[8]) << 13);
    x25 = ((uint64_t)(arg1[7]) << 5);
    x26 = ((uint64_t)(arg1[6]) << 48);
    x27 = ((uint64_t)(arg1[5]) << 40);
    x28 = ((uint64_t)(arg1[4]) << 32);
    x29 = ((uint64_t)(arg1[3]) << 24);
    x30 = ((uint64_t)(arg1[2]) << 16);
    x31 = ((uint64_t)(arg1[1]) << 8);
    x32 = (arg1[0]);
    x33 = (x31 + (uint64_t)x32);
    x34 = (x30 + x33);
    x35 = (x29 + x34);
    x36 = (x28 + x35);
    x37 = (x27 + x36);
    x38 = (x26 + x37);
    x39 = (x38 & UINT64_C(0x7ffffffffffff));
    x40 = (uint8_t)(x38 >> 51);
    x41 = (x25 + (uint64_t)x40);
    x42 = (x24 + x41);
    x43 = (x23 + x42);
    x44 = (x22 + x43);
    x45 = (x21 + x44);
    x46 = (x20 + x45);
    x47 = (x46 & UINT64_C(0x7ffffffffffff));
    x48 = (uint8_t)(x46 >> 51);
    x49 = (x19 + (uint64_t)x48);
    x50 = (x18 + x49);
    x51 = (x17 + x50);
    x52 = (x16 + x51);
    x53 = (x15 + x52);
    x54 = (x14 + x53);
    x55 = (x13 + x54);
    x56 = (x55 & UINT64_C(0x7ffffffffffff));
    x57 = (uint8_t)(x55 >> 51);
    x58 = (x12 + (uint64_t)x57);
    x59 = (x11 + x58);
    x60 = (x10 + x59);
    x61 = (x9 + x60);
    x62 = (x8 + x61);
    x63 = (x7 + x62);
    x64 = (x63 & UINT64_C(0x7ffffffffffff));
    x65 = (uint8_t)(x63 >> 51);
    x66 = (x6 + (uint64_t)x65);
    x67 = (x5 + x66);
    x68 = (x4 + x67);
    x69 = (x3 + x68);
    x70 = (x2 + x69);
    x71 = (x1 + x70);
    out1->v[0] = x39;
    out1->v[1] = x47;
    out1->v[2] = x56;
    out1->v[3] = x64;
    out1->v[4] = x71;
}

