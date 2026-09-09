#ifndef SPAKE2_FE_H
#define SPAKE2_FE_H

#include <stdint.h>

#define SPAKE2__FE_LIMB_COUNT 5

// fe means field element. Here the field is \Z/(2^255-19). An element t,
// entries t[0]...t[4], represents the integer t[0]+2^51 t[1]+2^102 t[2]+2^153
// t[3]+2^204 t[4].
// fe limbs are bounded by 1.125*2^51.
// Multiplication and carrying produce fe from fe_loose.
typedef struct fe {
    uint64_t v[SPAKE2__FE_LIMB_COUNT];
} spake2__fe_t;

// fe_loose limbs are bounded by 3.375*2^51.
// Addition and subtraction produce fe_loose from (fe, fe).
typedef struct fe_loose {
    uint64_t v[SPAKE2__FE_LIMB_COUNT];
} spake2__fe_loose_t;

void spake2__fe_0(
        spake2__fe_t *out1);

void spake2__fe_1(
        spake2__fe_t *out1);

void spake2__fe_copy(
        spake2__fe_t *out1, 
        const spake2__fe_t *arg1);

void spake2__fe_copy_lt(
        spake2__fe_loose_t *out1, 
        const spake2__fe_t *arg1);

void spake2__fe_loose_0(
        spake2__fe_loose_t *out1);

void spake2__fe_loose_1(
        spake2__fe_loose_t *out1);

void spake2__fe_loose_cmov(
        spake2__fe_loose_t *out1, 
        const spake2__fe_loose_t *arg1, 
        const uint64_t b);

int spake2__fe_loose_is_nonzero(
        const spake2__fe_loose_t *f);

int spake2__fe_is_negative(
        const spake2__fe_t *f);

void spake2__fe_neg(
        spake2__fe_loose_t *out1, 
        const spake2__fe_t *arg1);

void spake2__fe_add(
        spake2__fe_loose_t *out1, 
        const spake2__fe_t *arg1, 
        const spake2__fe_t *arg2);

void spake2__fe_sub(
        spake2__fe_loose_t *out1, 
        const spake2__fe_t *arg1, 
        const spake2__fe_t *arg2);

void spake2__fe_mul_llt(
        spake2__fe_loose_t *out1, 
        const spake2__fe_loose_t *arg1, 
        const spake2__fe_t *arg2);

void spake2__fe_mul_ltt(
        spake2__fe_loose_t *out1,
        const spake2__fe_t *arg1, 
        const spake2__fe_t *arg2);

void spake2__fe_mul_ttt(
        spake2__fe_t *out1, 
        const spake2__fe_t *arg1, 
        const spake2__fe_t *arg2);

void spake2__fe_mul_ttl(
        spake2__fe_t *out1, 
        const spake2__fe_t *arg1, 
        const spake2__fe_loose_t *arg2);

void spake2__fe_mul_tlt(
        spake2__fe_t *out1, 
        const spake2__fe_loose_t *arg1, 
        const spake2__fe_t *arg2);

void spake2__fe_mul_tll(
        spake2__fe_t *out1, 
        const spake2__fe_loose_t *arg1, 
        const spake2__fe_loose_t *arg2);

void spake2__fe_sq_tt(
        spake2__fe_t *h, 
        const spake2__fe_t *f);

void spake2__fe_sq_tl(
        spake2__fe_t *h, 
        const spake2__fe_loose_t *f);

void spake2__fe_sq2_tt(
        spake2__fe_t *h, 
        const spake2__fe_t *f);

void spake2__fe_pow22523(
        spake2__fe_t *out, 
        const spake2__fe_t *z);

void spake2__fe_invert(
        spake2__fe_t *out, 
        const spake2__fe_t *z);

void spake2__fe_carry(
        spake2__fe_t *out1, 
        const spake2__fe_loose_t *arg1);

void spake2__fe_from_bytes(
        spake2__fe_t *out1, 
        const uint8_t arg1[32]);

void spake2__fe_to_bytes(
        uint8_t out1[32], 
        const spake2__fe_t *arg1);

#endif
