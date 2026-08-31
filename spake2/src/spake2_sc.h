#ifndef SPAKE2_SC_H
#define SPAKE2_SC_H

#include <stdint.h>

#define SPAKE2__SC_LIMB_COUNT 32
#define SPAKE2__SC_WIDE_LIMB_COUNT 64

/*
    The set of scalars is \Z/l
    where l = 2^252 + 27742317777372353535851937790883648493.
*/

typedef uint8_t spake2__sc_t[SPAKE2__SC_LIMB_COUNT];
typedef uint8_t spake2__sc_wide_t[SPAKE2__SC_WIDE_LIMB_COUNT];

void spake2__sc_copy(
        spake2__sc_t s,
        const spake2__sc_t a);

void spake2__sc_reduce(
        spake2__sc_wide_t s);

void spake2__sc_muladd(
        spake2__sc_t s, 
        const spake2__sc_t a, 
        const spake2__sc_t b, 
        const spake2__sc_t c);

#endif
