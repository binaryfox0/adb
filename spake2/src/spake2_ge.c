#include "spake2_ge.h"
#include "spake2_ge_data.h"
#include "spake2_constt.h"

/*
r = p + q
*/

void spake2__ge_add(spake2__ge_p1p1 *r, const spake2__ge_p3 *p, const spake2__ge_cached *q) {
    spake2__fe_t t0;
    spake2__fe_add(r->X, p->Y, p->X);
    spake2__fe_sub(r->Y, p->Y, p->X);
    spake2__fe_mul(r->Z, r->X, q->YplusX);
    spake2__fe_mul(r->Y, r->Y, q->YminusX);
    spake2__fe_mul(r->T, q->T2d, p->T);
    spake2__fe_mul(r->X, p->Z, q->Z);
    spake2__fe_add(t0, r->X, r->X);
    spake2__fe_sub(r->X, r->Z, r->Y);
    spake2__fe_add(r->Y, r->Z, r->Y);
    spake2__fe_add(r->Z, t0, r->T);
    spake2__fe_sub(r->T, t0, r->T);
}


static void slide(signed char *r, const unsigned char *a) {
    int i;
    int b;
    int k;

    for (i = 0; i < 256; ++i) {
        r[i] = 1 & (a[i >> 3] >> (i & 7));
    }

    for (i = 0; i < 256; ++i)
        if (r[i]) {
            for (b = 1; b <= 6 && i + b < 256; ++b) {
                if (r[i + b]) {
                    if (r[i] + (r[i + b] << b) <= 15) {
                        r[i] += r[i + b] << b;
                        r[i + b] = 0;
                    } else if (r[i] - (r[i + b] << b) >= -15) {
                        r[i] -= r[i + b] << b;

                        for (k = i + b; k < 256; ++k) {
                            if (!r[k]) {
                                r[k] = 1;
                                break;
                            }

                            r[k] = 0;
                        }
                    } else {
                        break;
                    }
                }
            }
        }
}

/*
r = a * A + b * B
where a = a[0]+256*a[1]+...+256^31 a[31].
and b = b[0]+256*b[1]+...+256^31 b[31].
B is the Ed25519 base point (x,4/5) with x positive.
*/

void spake2__ge_double_scalarmult_vartime(spake2__ge_p2 *r, const unsigned char *a, const spake2__ge_p3 *A, const unsigned char *b) {
    signed char aslide[256];
    signed char bslide[256];
    spake2__ge_cached Ai[8]; /* A,3A,5A,7A,9A,11A,13A,15A */
    spake2__ge_p1p1 t;
    spake2__ge_p3 u;
    spake2__ge_p3 A2;
    int i;
    slide(aslide, a);
    slide(bslide, b);
    spake2__ge_p3_to_cached(&Ai[0], A);
    spake2__ge_p3_dbl(&t, A);
    spake2__ge_p1p1_to_p3(&A2, &t);
    spake2__ge_add(&t, &A2, &Ai[0]);
    spake2__ge_p1p1_to_p3(&u, &t);
    spake2__ge_p3_to_cached(&Ai[1], &u);
    spake2__ge_add(&t, &A2, &Ai[1]);
    spake2__ge_p1p1_to_p3(&u, &t);
    spake2__ge_p3_to_cached(&Ai[2], &u);
    spake2__ge_add(&t, &A2, &Ai[2]);
    spake2__ge_p1p1_to_p3(&u, &t);
    spake2__ge_p3_to_cached(&Ai[3], &u);
    spake2__ge_add(&t, &A2, &Ai[3]);
    spake2__ge_p1p1_to_p3(&u, &t);
    spake2__ge_p3_to_cached(&Ai[4], &u);
    spake2__ge_add(&t, &A2, &Ai[4]);
    spake2__ge_p1p1_to_p3(&u, &t);
    spake2__ge_p3_to_cached(&Ai[5], &u);
    spake2__ge_add(&t, &A2, &Ai[5]);
    spake2__ge_p1p1_to_p3(&u, &t);
    spake2__ge_p3_to_cached(&Ai[6], &u);
    spake2__ge_add(&t, &A2, &Ai[6]);
    spake2__ge_p1p1_to_p3(&u, &t);
    spake2__ge_p3_to_cached(&Ai[7], &u);
    spake2__ge_p2_0(r);

    for (i = 255; i >= 0; --i) {
        if (aslide[i] || bslide[i]) {
            break;
        }
    }

    for (; i >= 0; --i) {
        spake2__ge_p2_dbl(&t, r);

        if (aslide[i] > 0) {
            spake2__ge_p1p1_to_p3(&u, &t);
            spake2__ge_add(&t, &u, &Ai[aslide[i] / 2]);
        } else if (aslide[i] < 0) {
            spake2__ge_p1p1_to_p3(&u, &t);
            spake2__ge_sub(&t, &u, &Ai[(-aslide[i]) / 2]);
        }

        if (bslide[i] > 0) {
            spake2__ge_p1p1_to_p3(&u, &t);
            spake2__ge_madd(&t, &u, &Bi[bslide[i] / 2]);
        } else if (bslide[i] < 0) {
            spake2__ge_p1p1_to_p3(&u, &t);
            spake2__ge_msub(&t, &u, &Bi[(-bslide[i]) / 2]);
        }

        spake2__ge_p1p1_to_p2(r, &t);
    }
}


static const spake2__fe_t d = {
    -10913610, 13857413, -15372611, 6949391, 114729, -8787816, -6275908, -3247719, -18696448, -12055116
};

static const spake2__fe_t sqrtm1 = {
    -32595792, -7943725, 9377950, 3500415, 12389472, -272473, -25146209, -2005654, 326686, 11406482
};
int spake2__ge_frombytes_vartime(
        spake2__ge_p3 *h,
        const uint8_t s[32])
{
    spake2__fe_t u;
    spake2__fe_t v;
    spake2__fe_t w;
    spake2__fe_t vxx;
    spake2__fe_t check;

    spake2__fe_frombytes(h->Y, s);
    spake2__fe_1(h->Z);

    spake2__fe_sq(w, h->Y);
    spake2__fe_mul(vxx, w, d);

    spake2__fe_sub(v, w, h->Z);
    spake2__fe_copy(u, v);

    spake2__fe_add(v, vxx, h->Z);

    spake2__fe_mul(w, u, v);
    spake2__fe_pow22523(h->X, w);
    spake2__fe_mul(h->X, h->X, u);

    spake2__fe_sq(vxx, h->X);
    spake2__fe_mul(vxx, vxx, v);
    spake2__fe_sub(check, vxx, u);

    if (spake2__fe_isnonzero(check)) {
        spake2__fe_add(check, vxx, u);

        if (spake2__fe_isnonzero(check)) {
            return 0;
        }

        spake2__fe_mul(h->X, h->X, sqrtm1);
    }

    if (spake2__fe_isnegative(h->X) != (s[31] >> 7)) {
        spake2__fe_neg(h->X, h->X);
    }

    spake2__fe_mul(h->T, h->X, h->Y);

    return 1;
}
int spake2__ge_frombytes_negate_vartime(spake2__ge_p3 *h, const unsigned char *s) {
    spake2__fe_t u;
    spake2__fe_t v;
    spake2__fe_t v3;
    spake2__fe_t vxx;
    spake2__fe_t check;
    spake2__fe_frombytes(h->Y, s);
    spake2__fe_1(h->Z);
    spake2__fe_sq(u, h->Y);
    spake2__fe_mul(v, u, d);
    spake2__fe_sub(u, u, h->Z);     /* u = y^2-1 */
    spake2__fe_add(v, v, h->Z);     /* v = dy^2+1 */
    spake2__fe_sq(v3, v);
    spake2__fe_mul(v3, v3, v);      /* v3 = v^3 */
    spake2__fe_sq(h->X, v3);
    spake2__fe_mul(h->X, h->X, v);
    spake2__fe_mul(h->X, h->X, u);  /* x = uv^7 */
    spake2__fe_pow22523(h->X, h->X); /* x = (uv^7)^((q-5)/8) */
    spake2__fe_mul(h->X, h->X, v3);
    spake2__fe_mul(h->X, h->X, u);  /* x = uv^3(uv^7)^((q-5)/8) */
    spake2__fe_sq(vxx, h->X);
    spake2__fe_mul(vxx, vxx, v);
    spake2__fe_sub(check, vxx, u);  /* vx^2-u */

    if (spake2__fe_isnonzero(check)) {
        spake2__fe_add(check, vxx, u); /* vx^2+u */

        if (spake2__fe_isnonzero(check)) {
            return -1;
        }

        spake2__fe_mul(h->X, h->X, sqrtm1);
    }

    if (spake2__fe_isnegative(h->X) == (s[31] >> 7)) {
        spake2__fe_neg(h->X, h->X);
    }

    spake2__fe_mul(h->T, h->X, h->Y);
    return 0;
}


/*
r = p + q
*/

void spake2__ge_madd(spake2__ge_p1p1 *r, const spake2__ge_p3 *p, const spake2__ge_precomp *q) {
    spake2__fe_t t0;
    spake2__fe_add(r->X, p->Y, p->X);
    spake2__fe_sub(r->Y, p->Y, p->X);
    spake2__fe_mul(r->Z, r->X, q->yplusx);
    spake2__fe_mul(r->Y, r->Y, q->yminusx);
    spake2__fe_mul(r->T, q->xy2d, p->T);
    spake2__fe_add(t0, p->Z, p->Z);
    spake2__fe_sub(r->X, r->Z, r->Y);
    spake2__fe_add(r->Y, r->Z, r->Y);
    spake2__fe_add(r->Z, t0, r->T);
    spake2__fe_sub(r->T, t0, r->T);
}


/*
r = p - q
*/

void spake2__ge_msub(spake2__ge_p1p1 *r, const spake2__ge_p3 *p, const spake2__ge_precomp *q) {
    spake2__fe_t t0;

    spake2__fe_add(r->X, p->Y, p->X);
    spake2__fe_sub(r->Y, p->Y, p->X);
    spake2__fe_mul(r->Z, r->X, q->yminusx);
    spake2__fe_mul(r->Y, r->Y, q->yplusx);
    spake2__fe_mul(r->T, q->xy2d, p->T);
    spake2__fe_add(t0, p->Z, p->Z);
    spake2__fe_sub(r->X, r->Z, r->Y);
    spake2__fe_add(r->Y, r->Z, r->Y);
    spake2__fe_sub(r->Z, t0, r->T);
    spake2__fe_add(r->T, t0, r->T);
}


/*
r = p
*/

void spake2__ge_p1p1_to_p2(spake2__ge_p2 *r, const spake2__ge_p1p1 *p) {
    spake2__fe_mul(r->X, p->X, p->T);
    spake2__fe_mul(r->Y, p->Y, p->Z);
    spake2__fe_mul(r->Z, p->Z, p->T);
}



/*
r = p
*/

void spake2__ge_p1p1_to_p3(spake2__ge_p3 *r, const spake2__ge_p1p1 *p) {
    spake2__fe_mul(r->X, p->X, p->T);
    spake2__fe_mul(r->Y, p->Y, p->Z);
    spake2__fe_mul(r->Z, p->Z, p->T);
    spake2__fe_mul(r->T, p->X, p->Y);
}


void spake2__ge_p2_0(spake2__ge_p2 *h) {
    spake2__fe_0(h->X);
    spake2__fe_1(h->Y);
    spake2__fe_1(h->Z);
}



/*
r = 2 * p
*/

void spake2__ge_p2_dbl(spake2__ge_p1p1 *r, const spake2__ge_p2 *p) {
    spake2__fe_t t0;

    spake2__fe_sq(r->X, p->X);
    spake2__fe_sq(r->Z, p->Y);
    spake2__fe_sq2(r->T, p->Z);
    spake2__fe_add(r->Y, p->X, p->Y);
    spake2__fe_sq(t0, r->Y);
    spake2__fe_add(r->Y, r->Z, r->X);
    spake2__fe_sub(r->Z, r->Z, r->X);
    spake2__fe_sub(r->X, t0, r->Y);
    spake2__fe_sub(r->T, r->T, r->Z);
}


void spake2__ge_p3_0(spake2__ge_p3 *h) {
    spake2__fe_0(h->X);
    spake2__fe_1(h->Y);
    spake2__fe_1(h->Z);
    spake2__fe_0(h->T);
}


/*
r = 2 * p
*/

void spake2__ge_p3_dbl(spake2__ge_p1p1 *r, const spake2__ge_p3 *p) {
    spake2__ge_p2 q;
    spake2__ge_p3_to_p2(&q, p);
    spake2__ge_p2_dbl(r, &q);
}



/*
r = p
*/

static const spake2__fe_t d2 = {
    -21827239, -5839606, -30745221, 13898782, 229458, 15978800, -12551817, -6495438, 29715968, 9444199
};

void spake2__ge_p3_to_cached(spake2__ge_cached *r, const spake2__ge_p3 *p) {
    spake2__fe_add(r->YplusX, p->Y, p->X);
    spake2__fe_sub(r->YminusX, p->Y, p->X);
    spake2__fe_copy(r->Z, p->Z);
    spake2__fe_mul(r->T2d, p->T, d2);
}


/*
r = p
*/

void spake2__ge_p3_to_p2(spake2__ge_p2 *r, const spake2__ge_p3 *p) {
    spake2__fe_copy(r->X, p->X);
    spake2__fe_copy(r->Y, p->Y);
    spake2__fe_copy(r->Z, p->Z);
}


void spake2__ge_p3_tobytes(unsigned char *s, const spake2__ge_p3 *h) {
    spake2__fe_t recip;
    spake2__fe_t x;
    spake2__fe_t y;
    spake2__fe_invert(recip, h->Z);
    spake2__fe_mul(x, h->X, recip);
    spake2__fe_mul(y, h->Y, recip);
    spake2__fe_tobytes(s, y);
    s[31] ^= spake2__fe_isnegative(x) << 7;
}


static unsigned char equal(signed char b, signed char c) {
    unsigned char ub = b;
    unsigned char uc = c;
    unsigned char x = ub ^ uc; /* 0: yes; 1..255: no */
    uint64_t y = x; /* 0: yes; 1..255: no */
    y -= 1; /* large: yes; 0..254: no */
    y >>= 63; /* 1: yes; 0: no */
    return (unsigned char) y;
}

static unsigned char negative(signed char b) {
    uint64_t x = b; /* 18446744073709551361..18446744073709551615: yes; 0..255: no */
    x >>= 63; /* 1: yes; 0: no */
    return (unsigned char) x;
}

static void cmov(
        spake2__ge_precomp *t, 
        const spake2__ge_precomp *u, 
        unsigned char b) 
{
    spake2__fe_cmov(t->yplusx, u->yplusx, b);
    spake2__fe_cmov(t->yminusx, u->yminusx, b);
    spake2__fe_cmov(t->xy2d, u->xy2d, b);
}

static void cmov_cached(
        spake2__ge_cached *t, 
        const spake2__ge_cached *u, 
        unsigned char b)
{
    spake2__fe_cmov(t->YplusX, u->YplusX, b);
    spake2__fe_cmov(t->YminusX, u->YminusX, b);
    spake2__fe_cmov(t->Z, u->Z, b);
    spake2__fe_cmov(t->T2d, u->T2d, b);
}

static void select(spake2__ge_precomp *t, int pos, signed char b) {
    spake2__ge_precomp minust;
    unsigned char bnegative = negative(b);
    unsigned char babs = b - (((-bnegative) & b) << 1);
    spake2__fe_1(t->yplusx);
    spake2__fe_1(t->yminusx);
    spake2__fe_0(t->xy2d);
    cmov(t, &base[pos][0], equal(babs, 1));
    cmov(t, &base[pos][1], equal(babs, 2));
    cmov(t, &base[pos][2], equal(babs, 3));
    cmov(t, &base[pos][3], equal(babs, 4));
    cmov(t, &base[pos][4], equal(babs, 5));
    cmov(t, &base[pos][5], equal(babs, 6));
    cmov(t, &base[pos][6], equal(babs, 7));
    cmov(t, &base[pos][7], equal(babs, 8));
    spake2__fe_copy(minust.yplusx, t->yminusx);
    spake2__fe_copy(minust.yminusx, t->yplusx);
    spake2__fe_neg(minust.xy2d, t->xy2d);
    cmov(t, &minust, bnegative);
}
void spake2__ge_cached_0(
        spake2__ge_cached *h)
{
    spake2__fe_1(h->YplusX);
    spake2__fe_1(h->YminusX);
    spake2__fe_1(h->Z);
    spake2__fe_0(h->T2d);
}
static const spake2__fe_t spake2__fe_2d = {
    -21827239, 2773, -2867552, -5950665, -23321844,
    17502040, -13787687, -16624541, 14218417, -5960070
};
void spake2__ge_p1p1_to_cached(
        spake2__ge_cached *r,
        const spake2__ge_p1p1 *p)
{
    spake2__fe_t t;

    spake2__fe_add(r->YplusX, p->Y, p->X);
    spake2__fe_sub(r->YminusX, p->Y, p->X);
    spake2__fe_copy(r->Z, p->Z);

    spake2__fe_mul(t, p->T, spake2__fe_2d);
    spake2__fe_copy(r->T2d, t);
}

void spake2__ge_cached_cmov(
        spake2__ge_cached *f,
        const spake2__ge_cached *g,
        uint32_t b)
{
    spake2__fe_cmov(f->YplusX, g->YplusX, b);
    spake2__fe_cmov(f->YminusX, g->YminusX, b);
    spake2__fe_cmov(f->Z, g->Z, b);
    spake2__fe_cmov(f->T2d, g->T2d, b);
}
void spake2__ge_scalarmult(
        spake2__ge_p2 *r,
        const unsigned char *scalar,
        const spake2__ge_p3 *A)
{
    spake2__ge_p2 Ai_p2[8];
    spake2__ge_cached Ai[16];
    spake2__ge_p1p1 t;
    spake2__ge_p3 u;
    spake2__ge_cached selected;
    uint8_t index;
    unsigned i;
    unsigned j;

    spake2__ge_cached_0(&Ai[0]);
    spake2__ge_p3_to_cached(&Ai[1], A);
    spake2__ge_p3_to_p2(&Ai_p2[1], A);

    for (i = 2; i < 16; i += 2) {
        spake2__ge_p2_dbl(&t, &Ai_p2[i / 2]);
        spake2__ge_p1p1_to_cached(&Ai[i], &t);

        if (i < 8) {
            spake2__ge_p1p1_to_p2(&Ai_p2[i], &t);
        }

        spake2__ge_add(&t, A, &Ai[i]);
        spake2__ge_p1p1_to_cached(&Ai[i + 1], &t);

        if (i < 7) {
            spake2__ge_p1p1_to_p2(&Ai_p2[i + 1], &t);
        }
    }

    spake2__ge_p2_0(r);

    for (i = 0; i < 256; i += 4) {
        spake2__ge_p2_dbl(&t, r);
        spake2__ge_p1p1_to_p2(r, &t);

        spake2__ge_p2_dbl(&t, r);
        spake2__ge_p1p1_to_p2(r, &t);

        spake2__ge_p2_dbl(&t, r);
        spake2__ge_p1p1_to_p2(r, &t);

        spake2__ge_p2_dbl(&t, r);
        spake2__ge_p1p1_to_p3(&u, &t);

        index = scalar[31 - i / 8];
        index >>= 4 - (i & 4);
        index &= 0xf;

        spake2__ge_cached_0(&selected);

        for (j = 0; j < 16; ++j) {
            spake2__ge_cached_cmov(
                &selected,
                &Ai[j],
                1 & spake2__constant_time_eq_w(index, j));
        }

        spake2__ge_add(&t, &u, &selected);
        spake2__ge_p1p1_to_p2(r, &t);
    }
}

/*
h = a * B
where a = a[0]+256*a[1]+...+256^31 a[31]
B is the Ed25519 base point (x,4/5) with x positive.

Preconditions:
  a[31] <= 127
*/
void spake2__ge_scalarmult_base(spake2__ge_p3 *h, const unsigned char *a) {
    signed char e[64];
    signed char carry;
    spake2__ge_p1p1 r;
    spake2__ge_p2 s;
    spake2__ge_precomp t;
    int i;

    for (i = 0; i < 32; ++i) {
        e[2 * i + 0] = (a[i] >> 0) & 15;
        e[2 * i + 1] = (a[i] >> 4) & 15;
    }

    /* each e[i] is between 0 and 15 */
    /* e[63] is between 0 and 7 */
    carry = 0;

    for (i = 0; i < 63; ++i) {
        e[i] += carry;
        carry = e[i] + 8;
        carry >>= 4;
        e[i] -= carry << 4;
    }

    e[63] += carry;
    /* each e[i] is between -8 and 8 */
    spake2__ge_p3_0(h);

    for (i = 1; i < 64; i += 2) {
        select(&t, i / 2, e[i]);
        spake2__ge_madd(&r, h, &t);
        spake2__ge_p1p1_to_p3(h, &r);
    }

    spake2__ge_p3_dbl(&r, h);
    spake2__ge_p1p1_to_p2(&s, &r);
    spake2__ge_p2_dbl(&r, &s);
    spake2__ge_p1p1_to_p2(&s, &r);
    spake2__ge_p2_dbl(&r, &s);
    spake2__ge_p1p1_to_p2(&s, &r);
    spake2__ge_p2_dbl(&r, &s);
    spake2__ge_p1p1_to_p3(h, &r);

    for (i = 0; i < 64; i += 2) {
        select(&t, i / 2, e[i]);
        spake2__ge_madd(&r, h, &t);
        spake2__ge_p1p1_to_p3(h, &r);
    }
}

void spake2__ge_scalarmult_small_precomp(
        spake2__ge_p3 *h,
        const uint8_t a[32],
        const uint8_t precomp_table[15 * 2 * 32])
{
    spake2__ge_precomp multiples[15];
    spake2__ge_precomp e;
    spake2__ge_cached cached;
    spake2__ge_p1p1 r;
    spake2__fe_t x;
    spake2__fe_t y;

    for(int i = 0; i < 15; i++) 
    {
        const uint8_t *bytes;
        spake2__ge_precomp *out;

        bytes = &precomp_table[i * 64];
        out = &multiples[i];

        spake2__fe_frombytes(x, bytes);
        spake2__fe_frombytes(y, bytes + 32);

        spake2__fe_add(out->yplusx, y, x);
        spake2__fe_sub(out->yminusx, y, x);
        spake2__fe_mul(out->xy2d, x, y);
        spake2__fe_mul(out->xy2d, out->xy2d, d2);
    }

    spake2__ge_p3_0(h);
    for(int i = 63; i >= 0; i--) 
    {
        signed char index = 0;
        for(int j = 0; j < 4; j++) 
        {
            uint8_t bit = 1 & (a[(8 * j) + (i / 8)] >> (i & 7));
            index |= bit << j;
        }

        spake2__fe_1(e.yplusx);
        spake2__fe_1(e.yminusx);
        spake2__fe_0(e.xy2d);

        for(int j = 1; j < 16; j++) 
        {
            cmov(
                &e,
                &multiples[j - 1],
                equal(index, j));
        }

        spake2__ge_p3_to_cached(&cached, h);
        spake2__ge_add(&r, h, &cached);
        spake2__ge_p1p1_to_p3(h, &r);

        spake2__ge_madd(&r, h, &e);
        spake2__ge_p1p1_to_p3(h, &r);
    }
}

/*
r = p - q
*/

void spake2__ge_sub(
        spake2__ge_p1p1 *r, 
        const spake2__ge_p3 *p, 
        const spake2__ge_cached *q) 
{
    spake2__fe_t t0;
    
    spake2__fe_add(r->X, p->Y, p->X);
    spake2__fe_sub(r->Y, p->Y, p->X);
    spake2__fe_mul(r->Z, r->X, q->YminusX);
    spake2__fe_mul(r->Y, r->Y, q->YplusX);
    spake2__fe_mul(r->T, q->T2d, p->T);
    spake2__fe_mul(r->X, p->Z, q->Z);
    spake2__fe_add(t0, r->X, r->X);
    spake2__fe_sub(r->X, r->Z, r->Y);
    spake2__fe_add(r->Y, r->Z, r->Y);
    spake2__fe_sub(r->Z, t0, r->T);
    spake2__fe_add(r->T, t0, r->T);
}


void spake2__ge_tobytes(unsigned char *s, const spake2__ge_p2 *h) {
    spake2__fe_t recip;
    spake2__fe_t x;
    spake2__fe_t y;
    spake2__fe_invert(recip, h->Z);
    spake2__fe_mul(x, h->X, recip);
    spake2__fe_mul(y, h->Y, recip);
    spake2__fe_tobytes(s, y);
    s[31] ^= spake2__fe_isnegative(x) << 7;
}
