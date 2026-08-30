#ifndef SPAKE2_GE_H
#define SPAKE2_GE_H

#include "spake2_fe.h"


/*
spake2__ge means group element.

Here the group is the set of pairs (x,y) of field elements (see spake2_fe.h)
satisfying -x^2 + y^2 = 1 + d x^2y^2
where d = -121665/121666.

Representations:
    spake2__ge_p2 (projective): (X:Y:Z) satisfying x=X/Z, y=Y/Z
    spake2__ge_p3 (extended): (X:Y:Z:T) satisfying x=X/Z, y=Y/Z, XY=ZT
    spake2__ge_p1p1 (completed): ((X:Z),(Y:T)) satisfying x=X/Z, y=Y/T
    spake2__ge_precomp (Duif): (y+x,y-x,2dxy)
*/

typedef struct {
    spake2__fe_t X;
    spake2__fe_t Y;
    spake2__fe_t Z;
} spake2__ge_p2;

typedef struct {
    spake2__fe_t X;
    spake2__fe_t Y;
    spake2__fe_t Z;
    spake2__fe_t T;
} spake2__ge_p3;

typedef struct {
    spake2__fe_t X;
    spake2__fe_t Y;
    spake2__fe_t Z;
    spake2__fe_t T;
} spake2__ge_p1p1;

typedef struct {
    spake2__fe_t yplusx;
    spake2__fe_t yminusx;
    spake2__fe_t xy2d;
} spake2__ge_precomp;

typedef struct {
    spake2__fe_t YplusX;
    spake2__fe_t YminusX;
    spake2__fe_t Z;
    spake2__fe_t T2d;
} spake2__ge_cached;

void spake2__ge_p3_tobytes(
        unsigned char *s,
        const spake2__ge_p3 *h);

void spake2__ge_tobytes(
        unsigned char *s,
        const spake2__ge_p2 *h);

int spake2__ge_frombytes_negate_vartime(
        spake2__ge_p3 *h,
        const unsigned char *s);

void spake2__ge_add(
        spake2__ge_p1p1 *r,
        const spake2__ge_p3 *p,
        const spake2__ge_cached *q);

void spake2__ge_sub(
        spake2__ge_p1p1 *r,
        const spake2__ge_p3 *p,
        const spake2__ge_cached *q);

void spake2__ge_double_scalarmult_vartime(
        spake2__ge_p2 *r,
        const unsigned char *a,
        const spake2__ge_p3 *A,
        const unsigned char *b);

void spake2__ge_madd(
        spake2__ge_p1p1 *r,
        const spake2__ge_p3 *p,
        const spake2__ge_precomp *q);

void spake2__ge_msub(
        spake2__ge_p1p1 *r,
        const spake2__ge_p3 *p,
        const spake2__ge_precomp *q);

void spake2__ge_scalarmult_base(
        spake2__ge_p3 *h,
        const unsigned char *a);


void spake2__ge_p1p1_to_p2(
        spake2__ge_p2 *r,
        const spake2__ge_p1p1 *p);

void spake2__ge_p1p1_to_p3(
        spake2__ge_p3 *r,
        const spake2__ge_p1p1 *p);

void spake2__ge_p2_0(
        spake2__ge_p2 *h);

void spake2__ge_p2_dbl(
        spake2__ge_p1p1 *r,
        const spake2__ge_p2 *p);

void spake2__ge_p3_0(
        spake2__ge_p3 *h);

void spake2__ge_p3_dbl(
        spake2__ge_p1p1 *r,
        const spake2__ge_p3 *p);

void spake2__ge_p3_to_cached(
        spake2__ge_cached *r,
        const spake2__ge_p3 *p);

void spake2__ge_p3_to_p2(
        spake2__ge_p2 *r,
        const spake2__ge_p3 *p);


#endif
