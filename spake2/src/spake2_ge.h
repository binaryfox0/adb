#ifndef SPAKE2_GE_H
#define SPAKE2_GE_H

#include "spake2_fe.h"
#include "spake2_sc.h"

// ge means group element.
//
// Here the group is the set of pairs (x,y) of field elements (see fe.h)
// satisfying -x^2 + y^2 = 1 + d x^2y^2
// where d = -121665/121666.
//
// Representations:
//   ge_p2 (projective): (X:Y:Z) satisfying x=X/Z, y=Y/Z
//   ge_p3 (extended): (X:Y:Z:T) satisfying x=X/Z, y=Y/Z, XY=ZT
//   ge_p1p1 (completed): ((X:Z),(Y:T)) satisfying x=X/Z, y=Y/T
//   ge_precomp (Duif): (y+x,y-x,2dxy)

typedef struct {
    spake2__fe_t X;
    spake2__fe_t Y;
    spake2__fe_t Z;
} spake2__ge_p2_t;

typedef struct {
    spake2__fe_t X;
    spake2__fe_t Y;
    spake2__fe_t Z;
    spake2__fe_t T;
} spake2__ge_p3_t;

typedef struct {
    spake2__fe_loose_t X;
    spake2__fe_loose_t Y;
    spake2__fe_loose_t Z;
    spake2__fe_loose_t T;
} spake2__ge_p1p1_t;

typedef struct {
    spake2__fe_loose_t yplusx;
    spake2__fe_loose_t yminusx;
    spake2__fe_loose_t xy2d;
} spake2__ge_precomp_t;

typedef struct {
    spake2__fe_loose_t YplusX;
    spake2__fe_loose_t YminusX;
    spake2__fe_loose_t Z;
    spake2__fe_loose_t T2d;
} spake2__ge_cached_t;

void spake2__ge_p1p1_to_p2(
        spake2__ge_p2_t *out1, 
        const spake2__ge_p1p1_t *arg1);

void spake2__ge_p1p1_to_p3(
        spake2__ge_p3_t *out1, 
        const spake2__ge_p1p1_t *arg1); 

void spake2__ge_p3_to_cached(
        spake2__ge_cached_t *out1, 
        const spake2__ge_p3_t *arg1);

void spake2__ge_add(
        spake2__ge_p1p1_t *out1, 
        const spake2__ge_p3_t *arg1, 
        const spake2__ge_cached_t *arg2);

void spake2__ge_sub(
        spake2__ge_p1p1_t *out1, 
        const spake2__ge_p3_t *arg1, 
        const spake2__ge_cached_t *arg2);

void spake2__ge_scalarmult_small_precomp(
        spake2__ge_p3_t *h, 
        const uint8_t a[32], 
        const uint8_t precomp_table[15 * 2 * 32]);

void spake2__ge_scalarmult(
        spake2__ge_p2_t *r, 
        const spake2__sc_t *scalar,
        const spake2__ge_p3_t *A);

void spake2__ge_scalarmult_base(
        spake2__ge_p3_t *h, 
        const spake2__sc_t *a);

int spake2__ge_from_bytes_vartime(
        spake2__ge_p3_t *h, 
        const uint8_t s[32]);

void spake2__ge_to_bytes(
        uint8_t s[32], 
        const spake2__ge_p2_t *h);

#endif
