#include "spake2_ge.h"
#include "spake2_ge_data.h"

#include "spake2_ctime.h"

static void spake2__ge_p2_0(
        spake2__ge_p2_t *out1) 
{
   spake2__fe_0(&out1->X);
   spake2__fe_1(&out1->Y);
   spake2__fe_1(&out1->Z);
}
static void spake2__ge_p3_0(
        spake2__ge_p3_t *out1) 
{
    spake2__fe_0(&out1->X);
    spake2__fe_1(&out1->Y);
    spake2__fe_1(&out1->Z);
    spake2__fe_0(&out1->T);
}

static void spake2__ge_precomp_0(
        spake2__ge_precomp_t *out1) 
{
    spake2__fe_loose_1(&out1->yplusx);
    spake2__fe_loose_1(&out1->yminusx);
    spake2__fe_loose_0(&out1->xy2d);
}

static void spake2__ge_precomp_cmov(
        spake2__ge_precomp_t *out1, 
        const spake2__ge_precomp_t *arg1, 
        const uint64_t b) 
{
    spake2__fe_loose_cmov(&out1->yplusx, &arg1->yplusx, b);
    spake2__fe_loose_cmov(&out1->yminusx, &arg1->yminusx, b);
    spake2__fe_loose_cmov(&out1->xy2d, &arg1->xy2d, b);
}

static void spake2__ge_cached_0(
        spake2__ge_cached_t *out1) 
{
    spake2__fe_loose_1(&out1->YplusX);
    spake2__fe_loose_1(&out1->YminusX);
    spake2__fe_loose_1(&out1->Z);
    spake2__fe_loose_0(&out1->T2d);
}

// out1 = arg1
void spake2__ge_p1p1_to_p2(
        spake2__ge_p2_t *out1, 
        const spake2__ge_p1p1_t *arg1) 
{
    spake2__fe_mul_tll(&out1->X, &arg1->X, &arg1->T);
    spake2__fe_mul_tll(&out1->Y, &arg1->Y, &arg1->Z);
    spake2__fe_mul_tll(&out1->Z, &arg1->Z, &arg1->T);
}

// out1 = arg1
void spake2__ge_p1p1_to_p3(
        spake2__ge_p3_t *out1, 
        const spake2__ge_p1p1_t *arg1) 
{
    spake2__fe_mul_tll(&out1->X, &arg1->X, &arg1->T);
    spake2__fe_mul_tll(&out1->Y, &arg1->Y, &arg1->Z);
    spake2__fe_mul_tll(&out1->Z, &arg1->Z, &arg1->T);
    spake2__fe_mul_tll(&out1->T, &arg1->X, &arg1->Y);
}

// out1 = arg1
static void spake2__ge_p1p1_to_cached(
        spake2__ge_cached_t *out1, 
        const spake2__ge_p1p1_t *arg1) 
{
    spake2__ge_p3_t t;
    spake2__ge_p1p1_to_p3(&t, arg1);
    spake2__ge_p3_to_cached(out1, &t);
}

// out1 = arg1
static void spake2__ge_p3_to_p2(
        spake2__ge_p2_t *out1, 
        const spake2__ge_p3_t *arg1) 
{
    spake2__fe_copy(&out1->X, &arg1->X);
    spake2__fe_copy(&out1->Y, &arg1->Y);
    spake2__fe_copy(&out1->Z, &arg1->Z);
}

// out1 = arg1
void spake2__ge_p3_to_cached(
        spake2__ge_cached_t *out1, 
        const spake2__ge_p3_t *arg1) 
{
    spake2__fe_add(&out1->YplusX, &arg1->Y, &arg1->X);
    spake2__fe_sub(&out1->YminusX, &arg1->Y, &arg1->X);
    spake2__fe_copy_lt(&out1->Z, &arg1->Z);
    spake2__fe_mul_ltt(&out1->T2d, &arg1->T, &spake2__d2);
}


// out1 = arg1 + arg2
void spake2__ge_add(
        spake2__ge_p1p1_t *out1, 
        const spake2__ge_p3_t *arg1, 
        const spake2__ge_cached_t *arg2) 
{
  spake2__fe_t trX = {0}, trY = {0}, trZ = {0}, trT = {0};

  spake2__fe_add(&out1->X, &arg1->Y, &arg1->X);
  spake2__fe_sub(&out1->Y, &arg1->Y, &arg1->X);
  spake2__fe_mul_tll(&trZ, &out1->X, &arg2->YplusX);
  spake2__fe_mul_tll(&trY, &out1->Y, &arg2->YminusX);
  spake2__fe_mul_tlt(&trT, &arg2->T2d, &arg1->T);
  spake2__fe_mul_ttl(&trX, &arg1->Z, &arg2->Z);
  spake2__fe_add(&out1->T, &trX, &trX);
  spake2__fe_sub(&out1->X, &trZ, &trY);
  spake2__fe_add(&out1->Y, &trZ, &trY);
  spake2__fe_carry(&trZ, &out1->T);
  spake2__fe_add(&out1->Z, &trZ, &trT);
  spake2__fe_sub(&out1->T, &trZ, &trT);
}

// out1 = arg1 - arg2
void spake2__ge_sub(
        spake2__ge_p1p1_t *out1, 
        const spake2__ge_p3_t *arg1, 
        const spake2__ge_cached_t *arg2) 
{
    spake2__fe_t trX = {0}, trY = {0}, trZ = {0}, trT = {0};

    spake2__fe_add(&out1->X, &arg1->Y, &arg1->X);
    spake2__fe_sub(&out1->Y, &arg1->Y, &arg1->X);
    spake2__fe_mul_tll(&trZ, &out1->X, &arg2->YminusX);
    spake2__fe_mul_tll(&trY, &out1->Y, &arg2->YplusX);
    spake2__fe_mul_tlt(&trT, &arg2->T2d, &arg1->T);
    spake2__fe_mul_ttl(&trX, &arg1->Z, &arg2->Z);
    spake2__fe_add(&out1->T, &trX, &trX);
    spake2__fe_sub(&out1->X, &trZ, &trY);
    spake2__fe_add(&out1->Y, &trZ, &trY);
    spake2__fe_carry(&trZ, &out1->T);
    spake2__fe_sub(&out1->Z, &trZ, &trT);
    spake2__fe_add(&out1->T, &trZ, &trT);
}

// out1 = arg1 + arg2
static void spake2__ge_madd(
        spake2__ge_p1p1_t *out1, 
        const spake2__ge_p3_t *arg1, 
        const spake2__ge_precomp_t *arg2) 
{
  spake2__fe_t trY = {0}, trZ = {0}, trT = {0};

  spake2__fe_add(&out1->X, &arg1->Y, &arg1->X);
  spake2__fe_sub(&out1->Y, &arg1->Y, &arg1->X);
  spake2__fe_mul_tll(&trZ, &out1->X, &arg2->yplusx);
  spake2__fe_mul_tll(&trY, &out1->Y, &arg2->yminusx);
  spake2__fe_mul_tlt(&trT, &arg2->xy2d, &arg1->T);
  spake2__fe_add(&out1->T, &arg1->Z, &arg1->Z);
  spake2__fe_sub(&out1->X, &trZ, &trY);
  spake2__fe_add(&out1->Y, &trZ, &trY);
  spake2__fe_carry(&trZ, &out1->T);
  spake2__fe_add(&out1->Z, &trZ, &trT);
  spake2__fe_sub(&out1->T, &trZ, &trT);
}

// out1 = 2 * arg1
static void spake2__ge_p2_dbl(
        spake2__ge_p1p1_t *out1, 
        const spake2__ge_p2_t *arg1) 
{
    spake2__fe_t trX = {0}, trZ = {0}, trT = {0};
    spake2__fe_t t0 = {0};

    spake2__fe_sq_tt(&trX, &arg1->X);
    spake2__fe_sq_tt(&trZ, &arg1->Y);
    spake2__fe_sq2_tt(&trT, &arg1->Z);
    spake2__fe_add(&out1->Y, &arg1->X, &arg1->Y);
    spake2__fe_sq_tl(&t0, &out1->Y);

    spake2__fe_add(&out1->Y, &trZ, &trX);
    spake2__fe_sub(&out1->Z, &trZ, &trX);
    spake2__fe_carry(&trZ, &out1->Y);
    spake2__fe_sub(&out1->X, &t0, &trZ);
    spake2__fe_carry(&trZ, &out1->Z);
    spake2__fe_sub(&out1->T, &trT, &trZ);
}

static void spake2__ge_cached_cmov(
        spake2__ge_cached_t *t, 
        spake2__ge_cached_t *u, 
        uint8_t b) 
{
    spake2__fe_loose_cmov(&t->YplusX, &u->YplusX, b);
    spake2__fe_loose_cmov(&t->YminusX, &u->YminusX, b);
    spake2__fe_loose_cmov(&t->Z, &u->Z, b);
    spake2__fe_loose_cmov(&t->T2d, &u->T2d, b);
}

// r = scalar * A.
// where a = a[0]+256*a[1]+...+256^31 a[31].
void spake2__ge_scalarmult(
        spake2__ge_p2_t *out1, 
        const spake2__sc_t *scalar,
        const spake2__ge_p3_t *A) 
{
    spake2__ge_p2_t Ai_p2[8] = {0};
    spake2__ge_cached_t Ai[16] = {0};
    spake2__ge_p1p1_t t = {0};
    spake2__ge_p3_t u = {0};

    spake2__ge_cached_0(&Ai[0]);
    spake2__ge_p3_to_cached(&Ai[1], A);
    spake2__ge_p3_to_p2(&Ai_p2[1], A);

    for(uint32_t i = 2; i < 16; i += 2) 
    {
        spake2__ge_p2_dbl(&t, &Ai_p2[i / 2]);
        spake2__ge_p1p1_to_cached(&Ai[i], &t);
        if (i < 8)
            spake2__ge_p1p1_to_p2(&Ai_p2[i], &t);
        spake2__ge_add(&t, A, &Ai[i]);
        spake2__ge_p1p1_to_cached(&Ai[i + 1], &t);
        if (i < 7)
            spake2__ge_p1p1_to_p2(&Ai_p2[i + 1], &t);
    }

    spake2__ge_p2_0(out1);
    for(uint32_t i = 0; i < 256; i += 4) 
    {
        uint8_t index = scalar->v[31 - i / 8];
        spake2__ge_cached_t selected = {0};

        spake2__ge_p2_dbl(&t, out1);
        spake2__ge_p1p1_to_p2(out1, &t);
        spake2__ge_p2_dbl(&t, out1);
        spake2__ge_p1p1_to_p2(out1, &t);
        spake2__ge_p2_dbl(&t, out1);
        spake2__ge_p1p1_to_p2(out1, &t);
        spake2__ge_p2_dbl(&t, out1);
        spake2__ge_p1p1_to_p3(&u, &t);

        index >>= 4 - (i & 4);
        index &= 0xf;

        spake2__ge_cached_0(&selected);
        for(uint32_t j = 0; j < 16; j++) 
        {
            spake2__ge_cached_cmov(&selected, &Ai[j], 
                    1 & spake2__ctime_eq_w(index, j));
        }

        spake2__ge_add(&t, &u, &selected);
        spake2__ge_p1p1_to_p2(out1, &t);
    }
}

void spake2__ge_scalarmult_small_precomp(
        spake2__ge_p3_t *h, 
        const uint8_t a[32], 
        const uint8_t precomp_table[15 * 2 * 32]) 
{
    // precomp_table is first expanded into matching `ge_precomp`
    // elements.
    spake2__ge_precomp_t multiples[15] = {0};
    for(uint32_t i = 0; i < 15; i++)
    {
        // The precomputed table is assumed to already clear the top bit, so
        // `fe_frombytes_strict` may be used directly.
        const uint8_t *bytes = &precomp_table[i * (2 * 32)];
        spake2__fe_t x = {0}, y = {0};

        spake2__fe_from_bytes(&x, bytes);
        spake2__fe_from_bytes(&y, bytes + 32);

        spake2__ge_precomp_t *out = &multiples[i];
        spake2__fe_add(&out->yplusx, &y, &x);
        spake2__fe_sub(&out->yminusx, &y, &x);
        spake2__fe_mul_ltt(&out->xy2d, &x, &y);
        spake2__fe_mul_llt(&out->xy2d, &out->xy2d, &spake2__d2);
    }

    // See the comment above `k25519SmallPrecomp` about the structure of the
    // precomputed elements. This loop does 64 additions and 64 doublings to
    // calculate the result.
    spake2__ge_p3_0(h);
    for(uint32_t i = 63; i < 64; i--) 
    {
        uint64_t index = 0;
        spake2__ge_precomp_t e = {0};
        spake2__ge_cached_t cached = {0};
        spake2__ge_p1p1_t r = {0};

        for(uint32_t j = 0; j < 4; j++) 
        {
            const uint8_t bit = 1 & (a[(8 * j) + (i / 8)] >> (i & 7));
            index |= (uint64_t)(bit << j);
        }

        spake2__ge_precomp_0(&e);
        for(uint32_t j = 1; j < 16; j++) 
        {
            spake2__ge_precomp_cmov(&e, &multiples[j - 1], 
                    1 & spake2__ctime_eq_w(index, j));
        }

        spake2__ge_p3_to_cached(&cached, h);
        spake2__ge_add(&r, h, &cached);
        spake2__ge_p1p1_to_p3(h, &r);

        spake2__ge_madd(&r, h, &e);
        spake2__ge_p1p1_to_p3(h, &r);
    }
}

void spake2__ge_scalarmult_base(
        spake2__ge_p3_t *h, 
        const spake2__sc_t *a) {
    spake2__ge_scalarmult_small_precomp(h, a->v, 
            spake2__small_precomp);
}

int spake2__ge_from_bytes_vartime(
        spake2__ge_p3_t *h, 
        const uint8_t s[32]) 
{
    spake2__fe_t u = {0};
    spake2__fe_loose_t v = {0};
    spake2__fe_t w = {0};
    spake2__fe_t vxx = {0};
    spake2__fe_loose_t check = {0};

    spake2__fe_from_bytes(&h->Y, s);
    spake2__fe_1(&h->Z);
    spake2__fe_sq_tt(&w, &h->Y);
    spake2__fe_mul_ttt(&vxx, &w, &spake2__d);
    spake2__fe_sub(&v, &w, &h->Z);        // u = y^2-1
    spake2__fe_carry(&u, &v);
    spake2__fe_add(&v, &vxx, &h->Z);      // v = dy^2+1

    spake2__fe_mul_ttl(&w, &u, &v);       // w = u*v
    spake2__fe_pow22523(&h->X, &w);                 // x = w^((q-5)/8)
    spake2__fe_mul_ttt(&h->X, &h->X, &u); // x = u*w^((q-5)/8)

    spake2__fe_sq_tt(&vxx, &h->X);
    spake2__fe_mul_ttl(&vxx, &vxx, &v);
    spake2__fe_sub(&check, &vxx, &u);
    if (spake2__fe_loose_is_nonzero(&check)) 
    {
        spake2__fe_add(&check, &vxx, &u);
        if (spake2__fe_loose_is_nonzero(&check))
            return 0;
        spake2__fe_mul_ttt(&h->X, &h->X, &spake2__sqrtm1);
    }

    if (spake2__fe_is_negative(&h->X) != (s[31] >> 7)) 
    {
        spake2__fe_loose_t t = {0};
        spake2__fe_neg(&t, &h->X);
        spake2__fe_carry(&h->X, &t);
    }

    spake2__fe_mul_ttt(&h->T, &h->X, &h->Y);
    return 1;
}

void spake2__ge_to_bytes(
        uint8_t s[32], 
        const spake2__ge_p2_t *h) 
{
    spake2__fe_t recip = {0};
    spake2__fe_t x = {0};
    spake2__fe_t y = {0};

    spake2__fe_invert(&recip, &h->Z);
    spake2__fe_mul_ttt(&x, &h->X, &recip);
    spake2__fe_mul_ttt(&y, &h->Y, &recip);
    spake2__fe_to_bytes(s, &y);
    s[31] ^= spake2__fe_is_negative(&x) << 7;
}
