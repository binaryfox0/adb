#ifndef SPAKE2_FE_H
#define SPAKE2_FE_H

#include <stdint.h>

#define SPAKE2__FE_LIMB_COUNT 10

/*
    fe means field element.
    Here the field is \Z/(2^255-19).
    An element t, entries t[0]...t[9], represents the integer
    t[0]+2^26 t[1]+2^51 t[2]+2^77 t[3]+2^102 t[4]+...+2^230 t[9].
    Bounds on each t[i] vary depending on context.
*/


typedef int32_t spake2__fe_t[SPAKE2__FE_LIMB_COUNT];


void spake2__fe_0(
        spake2__fe_t h);

void spake2__fe_1(
        spake2__fe_t h);

void spake2__fe_frombytes(
        spake2__fe_t h, 
        const unsigned char *s);

void spake2__fe_tobytes(
        unsigned char *s, 
        const spake2__fe_t h);

void spake2__fe_copy(
        spake2__fe_t h, 
        const spake2__fe_t f);

int spake2__fe_isnegative(
        const spake2__fe_t f);

int spake2__fe_isnonzero(
        const spake2__fe_t f);

void spake2__fe_cmov(
        spake2__fe_t f, 
        const spake2__fe_t g,
        unsigned int b);

void spake2__fe_cswap(
        spake2__fe_t f, 
        spake2__fe_t g, 
        unsigned int b);

void spake2__fe_neg(
        spake2__fe_t h, 
        const spake2__fe_t f);

void spake2__fe_add(
        spake2__fe_t h, 
        const spake2__fe_t f, 
        const spake2__fe_t g);

void spake2__fe_invert(
        spake2__fe_t out, 
        const spake2__fe_t z);

void spake2__fe_sq(
        spake2__fe_t h, 
        const spake2__fe_t f);

void spake2__fe_sq2(
        spake2__fe_t h, 
        const spake2__fe_t f);

void spake2__fe_mul(
        spake2__fe_t h, 
        const spake2__fe_t f, 
        const spake2__fe_t g);

void spake2__fe_mul121666(
        spake2__fe_t h, 
        spake2__fe_t f);

void spake2__fe_pow22523(
        spake2__fe_t out, 
        const spake2__fe_t z);

void spake2__fe_sub(
        spake2__fe_t h, 
        const spake2__fe_t f, 
        const spake2__fe_t g);

#endif
