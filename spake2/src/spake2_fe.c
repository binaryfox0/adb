#include "spake2_fe.h"

#include <string.h>

static uint64_t spake2__load3(
        const uint8_t *in) 
{
    return 
        (uint64_t) in[0] |
        ((uint64_t) in[1]) << 8 |
        ((uint64_t) in[2]) << 16;
}

static uint64_t spake2__load4(
        const uint8_t *in) 
{
    return 
        (uint64_t)in[0] |
        ((uint64_t)in[1]) << 8 |
        ((uint64_t)in[2]) << 16 |
        ((uint64_t)in[3]) << 24;
}

static void spake2__fe_carry(
        int64_t h[SPAKE2__FE_LIMB_COUNT])
{
    int64_t carry = 0;

    carry = (h[9] + (int64_t)(1 << 24)) >> 25; 
    h[0] += carry * 19; h[9] -= carry << 25;
    carry = (h[1] + (int64_t)(1 << 24)) >> 25; 
    h[2] += carry, h[1] -= carry << 25;
    carry = (h[3] + (int64_t)(1 << 24)) >> 25; 
    h[4] += carry, h[3] -= carry << 25;
    carry = (h[5] + (int64_t)(1 << 24)) >> 25; 
    h[6] += carry, h[5] -= carry << 25;
    carry = (h[7] + (int64_t)(1 << 24)) >> 25; 
    h[8] += carry, h[7] -= carry << 25;
    carry = (h[0] + (int64_t)(1 << 25)) >> 26; 
    h[1] += carry, h[0] -= carry << 26;
    carry = (h[2] + (int64_t)(1 << 25)) >> 26; 
    h[3] += carry, h[2] -= carry << 26;
    carry = (h[4] + (int64_t)(1 << 25)) >> 26; 
    h[5] += carry, h[4] -= carry << 26;
    carry = (h[6] + (int64_t)(1 << 25)) >> 26; 
    h[7] += carry, h[6] -= carry << 26;
    carry = (h[8] + (int64_t)(1 << 25)) >> 26; 
    h[9] += carry, h[8] -= carry << 26;
}

void spake2__fe_0(spake2__fe_t h) 
{
    memset(h, 0, sizeof(spake2__fe_t));
}


void spake2__fe_1(spake2__fe_t h) 
{
    memset(h, 0, sizeof(spake2__fe_t));
    h[0] = 1;
}



/*
    h = f + g
    Can overlap h with f or g.

    Preconditions:
       |f| bounded by 1.1*2^25,1.1*2^24,1.1*2^25,1.1*2^24,etc.
       |g| bounded by 1.1*2^25,1.1*2^24,1.1*2^25,1.1*2^24,etc.

    Postconditions:
       |h| bounded by 1.1*2^26,1.1*2^25,1.1*2^26,1.1*2^25,etc.
*/

void spake2__fe_add(
        spake2__fe_t h, 
        const spake2__fe_t f, 
        const spake2__fe_t g)
{
    for(int i = 0; i < SPAKE2__FE_LIMB_COUNT; i++)
        h[i] = f[i] + g[i];
}



/*
    Replace (f,g) with (g,g) if b == 1;
    replace (f,g) with (f,g) if b == 0.

    Preconditions: b in {0,1}.
*/

void spake2__fe_cmov(spake2__fe_t f, const spake2__fe_t g, unsigned int b)
{
    uint32_t mask = (unsigned int)(-(int)b);
    for(int i = 0; i < 10; i++) 
    {
        int32_t x = f[i] ^ g[i];
        x &= (int32_t)mask;
        f[i] ^= x;
    }
}

/*
    Replace (f,g) with (g,f) if b == 1;
    replace (f,g) with (f,g) if b == 0.

    Preconditions: b in {0,1}.
*/

void spake2__fe_cswap(spake2__fe_t f, spake2__fe_t g, unsigned int b)
{
    uint32_t mask = (unsigned int)(-(int)b);
    for(int i = 0; i < 10; i++) 
    {
        int32_t x = f[i] ^ g[i];
        x &= (int32_t)mask;
        f[i] ^= x;
        g[i] ^= x;
    }
}

void spake2__fe_copy(
        spake2__fe_t h, 
        const spake2__fe_t f) 
{
    memcpy(h, f, sizeof(spake2__fe_t));
}



/*
    Ignores top bit of h.
*/

void spake2__fe_frombytes(
        spake2__fe_t h, 
        const uint8_t *s) 
{
    int64_t w[SPAKE2__FE_LIMB_COUNT] = {0};

    w[0] = (int64_t)spake2__load4(s);
    w[1] = (int64_t)(spake2__load3(s + 4) << 6);
    w[2] = (int64_t)(spake2__load3(s + 7) << 5);
    w[3] = (int64_t)(spake2__load3(s + 10) << 3);
    w[4] = (int64_t)(spake2__load3(s + 13) << 2);
    w[5] = (int64_t)spake2__load4(s + 16);
    w[6] = (int64_t)(spake2__load3(s + 20) << 7);
    w[7] = (int64_t)(spake2__load3(s + 23) << 5);
    w[8] = (int64_t)(spake2__load3(s + 26) << 4);
    w[9] = (int64_t)((spake2__load3(s + 29) & 8388607) << 2);

    spake2__fe_carry(w);

    for(int i = 0; i < SPAKE2__FE_LIMB_COUNT; i++)
        h[i] = (int32_t)w[i];
}

static void spake2__fe_sq_n(
        spake2__fe_t out,
        const spake2__fe_t in,
        const uint32_t n)
{
    spake2__fe_copy(out, in);
    for(uint32_t i = 0; i < n; ++i)
        spake2__fe_sq(out, out);
}

void spake2__fe_invert(
        spake2__fe_t out,
        const spake2__fe_t z)
{
    spake2__fe_t t0 = {0};
    spake2__fe_t t1 = {0};
    spake2__fe_t t2 = {0};
    spake2__fe_t t3 = {0};

    spake2__fe_sq(t0, z);
    spake2__fe_sq(t1, t0);
    spake2__fe_sq_n(t1, t1, 1);

    spake2__fe_mul(t1, z, t1);
    spake2__fe_mul(t0, t0, t1);
    spake2__fe_sq(t2, t0);

    spake2__fe_mul(t1, t1, t2);
    spake2__fe_sq(t2, t1);
    spake2__fe_sq_n(t2, t2, 4);

    spake2__fe_mul(t1, t2, t1);
    spake2__fe_sq(t2, t1);
    spake2__fe_sq_n(t2, t2, 9);

    spake2__fe_mul(t2, t2, t1);
    spake2__fe_sq(t3, t2);
    spake2__fe_sq_n(t3, t3, 19);

    spake2__fe_mul(t2, t3, t2);
    spake2__fe_sq(t2, t2);
    spake2__fe_sq_n(t2, t2, 9);

    spake2__fe_mul(t1, t2, t1);
    spake2__fe_sq(t2, t1);
    spake2__fe_sq_n(t2, t2, 49);

    spake2__fe_mul(t2, t2, t1);
    spake2__fe_sq(t3, t2);
    spake2__fe_sq_n(t3, t3, 99);

    spake2__fe_mul(t2, t3, t2);
    spake2__fe_sq(t2, t2);
    spake2__fe_sq_n(t2, t2, 49);

    spake2__fe_mul(t1, t2, t1);
    spake2__fe_sq(t1, t1);
    spake2__fe_sq_n(t1, t1, 4);

    spake2__fe_mul(out, t1, t0);
}


/*
    return 1 if f is in {1,3,5,...,q-2}
    return 0 if f is in {0,2,4,...,q-1}

    Preconditions:
       |f| bounded by 1.1*2^26,1.1*2^25,1.1*2^26,1.1*2^25,etc.
*/

int spake2__fe_isnegative(
        const spake2__fe_t f) 
{
    uint8_t s[32] = {0};
    spake2__fe_tobytes(s, f);
    return s[0] & 1;
}



/*
    return 1 if f == 0
    return 0 if f != 0

    Preconditions:
       |f| bounded by 1.1*2^26,1.1*2^25,1.1*2^26,1.1*2^25,etc.
*/

int spake2__fe_isnonzero(const spake2__fe_t f) {
    unsigned char s[32];
    unsigned char r;

    spake2__fe_tobytes(s, f);

    r = s[0];
    #define F(i) r |= s[i]
    F(1);
    F(2);
    F(3);
    F(4);
    F(5);
    F(6);
    F(7);
    F(8);
    F(9);
    F(10);
    F(11);
    F(12);
    F(13);
    F(14);
    F(15);
    F(16);
    F(17);
    F(18);
    F(19);
    F(20);
    F(21);
    F(22);
    F(23);
    F(24);
    F(25);
    F(26);
    F(27);
    F(28);
    F(29);
    F(30);
    F(31);
    #undef F

    return r != 0;
}



/*
    h = f * g
    Can overlap h with f or g.

    Preconditions:
       |f| bounded by 1.65*2^26,1.65*2^25,1.65*2^26,1.65*2^25,etc.
       |g| bounded by 1.65*2^26,1.65*2^25,1.65*2^26,1.65*2^25,etc.

    Postconditions:
       |h| bounded by 1.01*2^25,1.01*2^24,1.01*2^25,1.01*2^24,etc.
    */

    /*
    Notes on implementation strategy:

    Using schoolbook multiplication.
    Karatsuba would save a little in some cost models.

    Most multiplications by 2 and 19 are 32-bit precomputations;
    cheaper than 64-bit postcomputations.

    There is one remaining multiplication by 19 in the carry chain;
    one *19 precomputation can be merged into this,
    but the resulting data flow is considerably less clean.

    There are 12 carries below.
    10 of them are 2-way parallelizable and vectorizable.
    Can get away with 11 carries, but then data flow is much deeper.

    With tighter constraints on inputs can squeeze carries into int32.
*/

void spake2__fe_mul(
        spake2__fe_t h,
        const spake2__fe_t f,
        const spake2__fe_t g)
{
    int64_t x[SPAKE2__FE_LIMB_COUNT] = {0};
    int64_t carry = 0;
    int i;
    int j;
    int k;
    int factor;

    for (i = 0; i < SPAKE2__FE_LIMB_COUNT; ++i) {
        for (j = 0; j < SPAKE2__FE_LIMB_COUNT; ++j) {
            k = i + j;
            factor = 1;

            /*
             * Limb bases are:
             *
             *   2^0, 2^26, 2^51, 2^77, ...
             *
             * When both indices are odd, the product is one extra
             * factor of 2 relative to the target limb.
             */
            if ((i & 1) && (j & 1))
                factor = 2;

            /*
             * 2^255 == 19 (mod 2^255 - 19)
             */
            if (k >= SPAKE2__FE_LIMB_COUNT) 
            {
                k -= SPAKE2__FE_LIMB_COUNT;
                factor *= 19;
            }

            x[k] += (int64_t)f[i] * g[j] * factor;
        }
    }

    carry = (x[0] + ((int64_t)1 << 25)) >> 26;
    x[1] += carry;
    x[0] -= carry << 26;

    carry = (x[1] + ((int64_t)1 << 24)) >> 25;
    x[2] += carry;
    x[1] -= carry << 25;

    carry = (x[2] + ((int64_t)1 << 25)) >> 26;
    x[3] += carry;
    x[2] -= carry << 26;

    carry = (x[3] + ((int64_t)1 << 24)) >> 25;
    x[4] += carry;
    x[3] -= carry << 25;

    carry = (x[4] + ((int64_t)1 << 25)) >> 26;
    x[5] += carry;
    x[4] -= carry << 26;

    carry = (x[5] + ((int64_t)1 << 24)) >> 25;
    x[6] += carry;
    x[5] -= carry << 25;

    carry = (x[6] + ((int64_t)1 << 25)) >> 26;
    x[7] += carry;
    x[6] -= carry << 26;

    carry = (x[7] + ((int64_t)1 << 24)) >> 25;
    x[8] += carry;
    x[7] -= carry << 25;

    carry = (x[8] + ((int64_t)1 << 25)) >> 26;
    x[9] += carry;
    x[8] -= carry << 26;

    carry = (x[9] + ((int64_t)1 << 24)) >> 25;
    x[0] += carry * 19;
    x[9] -= carry << 25;

    carry = (x[0] + ((int64_t)1 << 25)) >> 26;
    x[1] += carry;
    x[0] -= carry << 26;

    for (i = 0; i < SPAKE2__FE_LIMB_COUNT; ++i)
        h[i] = (int32_t)x[i];
}

/*
h = -f

Preconditions:
   |f| bounded by 1.1*2^25,1.1*2^24,1.1*2^25,1.1*2^24,etc.

Postconditions:
   |h| bounded by 1.1*2^25,1.1*2^24,1.1*2^25,1.1*2^24,etc.
*/

void spake2__fe_neg(
        spake2__fe_t h, 
        const spake2__fe_t f) 
{
    for(int i = 0; i < SPAKE2__FE_LIMB_COUNT; i++)
        h[i] = -f[i];
}

/*
h = f * f
Can overlap h with f.

Preconditions:
   |f| bounded by 1.65*2^26,1.65*2^25,1.65*2^26,1.65*2^25,etc.

Postconditions:
   |h| bounded by 1.01*2^25,1.01*2^24,1.01*2^25,1.01*2^24,etc.
*/

/*
See spake2__fe_mul.c for discussion of implementation strategy.
*/

void spake2__fe_sq(spake2__fe_t h, const spake2__fe_t f) {
    int32_t f0 = f[0];
    int32_t f1 = f[1];
    int32_t f2 = f[2];
    int32_t f3 = f[3];
    int32_t f4 = f[4];
    int32_t f5 = f[5];
    int32_t f6 = f[6];
    int32_t f7 = f[7];
    int32_t f8 = f[8];
    int32_t f9 = f[9];
    int32_t f0_2 = 2 * f0;
    int32_t f1_2 = 2 * f1;
    int32_t f2_2 = 2 * f2;
    int32_t f3_2 = 2 * f3;
    int32_t f4_2 = 2 * f4;
    int32_t f5_2 = 2 * f5;
    int32_t f6_2 = 2 * f6;
    int32_t f7_2 = 2 * f7;
    int32_t f5_38 = 38 * f5; /* 1.959375*2^30 */
    int32_t f6_19 = 19 * f6; /* 1.959375*2^30 */
    int32_t f7_38 = 38 * f7; /* 1.959375*2^30 */
    int32_t f8_19 = 19 * f8; /* 1.959375*2^30 */
    int32_t f9_38 = 38 * f9; /* 1.959375*2^30 */
    int64_t f0f0    = f0   * (int64_t) f0;
    int64_t f0f1_2  = f0_2 * (int64_t) f1;
    int64_t f0f2_2  = f0_2 * (int64_t) f2;
    int64_t f0f3_2  = f0_2 * (int64_t) f3;
    int64_t f0f4_2  = f0_2 * (int64_t) f4;
    int64_t f0f5_2  = f0_2 * (int64_t) f5;
    int64_t f0f6_2  = f0_2 * (int64_t) f6;
    int64_t f0f7_2  = f0_2 * (int64_t) f7;
    int64_t f0f8_2  = f0_2 * (int64_t) f8;
    int64_t f0f9_2  = f0_2 * (int64_t) f9;
    int64_t f1f1_2  = f1_2 * (int64_t) f1;
    int64_t f1f2_2  = f1_2 * (int64_t) f2;
    int64_t f1f3_4  = f1_2 * (int64_t) f3_2;
    int64_t f1f4_2  = f1_2 * (int64_t) f4;
    int64_t f1f5_4  = f1_2 * (int64_t) f5_2;
    int64_t f1f6_2  = f1_2 * (int64_t) f6;
    int64_t f1f7_4  = f1_2 * (int64_t) f7_2;
    int64_t f1f8_2  = f1_2 * (int64_t) f8;
    int64_t f1f9_76 = f1_2 * (int64_t) f9_38;
    int64_t f2f2    = f2   * (int64_t) f2;
    int64_t f2f3_2  = f2_2 * (int64_t) f3;
    int64_t f2f4_2  = f2_2 * (int64_t) f4;
    int64_t f2f5_2  = f2_2 * (int64_t) f5;
    int64_t f2f6_2  = f2_2 * (int64_t) f6;
    int64_t f2f7_2  = f2_2 * (int64_t) f7;
    int64_t f2f8_38 = f2_2 * (int64_t) f8_19;
    int64_t f2f9_38 = f2   * (int64_t) f9_38;
    int64_t f3f3_2  = f3_2 * (int64_t) f3;
    int64_t f3f4_2  = f3_2 * (int64_t) f4;
    int64_t f3f5_4  = f3_2 * (int64_t) f5_2;
    int64_t f3f6_2  = f3_2 * (int64_t) f6;
    int64_t f3f7_76 = f3_2 * (int64_t) f7_38;
    int64_t f3f8_38 = f3_2 * (int64_t) f8_19;
    int64_t f3f9_76 = f3_2 * (int64_t) f9_38;
    int64_t f4f4    = f4   * (int64_t) f4;
    int64_t f4f5_2  = f4_2 * (int64_t) f5;
    int64_t f4f6_38 = f4_2 * (int64_t) f6_19;
    int64_t f4f7_38 = f4   * (int64_t) f7_38;
    int64_t f4f8_38 = f4_2 * (int64_t) f8_19;
    int64_t f4f9_38 = f4   * (int64_t) f9_38;
    int64_t f5f5_38 = f5   * (int64_t) f5_38;
    int64_t f5f6_38 = f5_2 * (int64_t) f6_19;
    int64_t f5f7_76 = f5_2 * (int64_t) f7_38;
    int64_t f5f8_38 = f5_2 * (int64_t) f8_19;
    int64_t f5f9_76 = f5_2 * (int64_t) f9_38;
    int64_t f6f6_19 = f6   * (int64_t) f6_19;
    int64_t f6f7_38 = f6   * (int64_t) f7_38;
    int64_t f6f8_38 = f6_2 * (int64_t) f8_19;
    int64_t f6f9_38 = f6   * (int64_t) f9_38;
    int64_t f7f7_38 = f7   * (int64_t) f7_38;
    int64_t f7f8_38 = f7_2 * (int64_t) f8_19;
    int64_t f7f9_76 = f7_2 * (int64_t) f9_38;
    int64_t f8f8_19 = f8   * (int64_t) f8_19;
    int64_t f8f9_38 = f8   * (int64_t) f9_38;
    int64_t f9f9_38 = f9   * (int64_t) f9_38;
    int64_t h0 = f0f0  + f1f9_76 + f2f8_38 + f3f7_76 + f4f6_38 + f5f5_38;
    int64_t h1 = f0f1_2 + f2f9_38 + f3f8_38 + f4f7_38 + f5f6_38;
    int64_t h2 = f0f2_2 + f1f1_2 + f3f9_76 + f4f8_38 + f5f7_76 + f6f6_19;
    int64_t h3 = f0f3_2 + f1f2_2 + f4f9_38 + f5f8_38 + f6f7_38;
    int64_t h4 = f0f4_2 + f1f3_4 + f2f2   + f5f9_76 + f6f8_38 + f7f7_38;
    int64_t h5 = f0f5_2 + f1f4_2 + f2f3_2 + f6f9_38 + f7f8_38;
    int64_t h6 = f0f6_2 + f1f5_4 + f2f4_2 + f3f3_2 + f7f9_76 + f8f8_19;
    int64_t h7 = f0f7_2 + f1f6_2 + f2f5_2 + f3f4_2 + f8f9_38;
    int64_t h8 = f0f8_2 + f1f7_4 + f2f6_2 + f3f5_4 + f4f4   + f9f9_38;
    int64_t h9 = f0f9_2 + f1f8_2 + f2f7_2 + f3f6_2 + f4f5_2;
    int64_t carry;

    carry = (h0 + (int64_t)(1 << 25)) >> 26;
    h1 += carry;
    h0 -= carry << 26;
    carry = (h4 + (int64_t)(1 << 25)) >> 26;
    h5 += carry;
    h4 -= carry << 26;
    carry = (h1 + (int64_t)(1 << 24)) >> 25;
    h2 += carry;
    h1 -= carry << 25;
    carry = (h5 + (int64_t)(1 << 24)) >> 25;
    h6 += carry;
    h5 -= carry << 25;
    carry = (h2 + (int64_t)(1 << 25)) >> 26;
    h3 += carry;
    h2 -= carry << 26;
    carry = (h6 + (int64_t)(1 << 25)) >> 26;
    h7 += carry;
    h6 -= carry << 26;
    carry = (h3 + (int64_t)(1 << 24)) >> 25;
    h4 += carry;
    h3 -= carry << 25;
    carry = (h7 + (int64_t)(1 << 24)) >> 25;
    h8 += carry;
    h7 -= carry << 25;
    carry = (h4 + (int64_t)(1 << 25)) >> 26;
    h5 += carry;
    h4 -= carry << 26;
    carry = (h8 + (int64_t)(1 << 25)) >> 26;
    h9 += carry;
    h8 -= carry << 26;
    carry = (h9 + (int64_t)(1 << 24)) >> 25;
    h0 += carry * 19;
    h9 -= carry << 25;
    carry = (h0 + (int64_t)(1 << 25)) >> 26;
    h1 += carry;
    h0 -= carry << 26;
    h[0] = (int32_t) h0;
    h[1] = (int32_t) h1;
    h[2] = (int32_t) h2;
    h[3] = (int32_t) h3;
    h[4] = (int32_t) h4;
    h[5] = (int32_t) h5;
    h[6] = (int32_t) h6;
    h[7] = (int32_t) h7;
    h[8] = (int32_t) h8;
    h[9] = (int32_t) h9;
}

/*
h = f - g
Can overlap h with f or g.

Preconditions:
   |f| bounded by 1.1*2^25,1.1*2^24,1.1*2^25,1.1*2^24,etc.
   |g| bounded by 1.1*2^25,1.1*2^24,1.1*2^25,1.1*2^24,etc.

Postconditions:
   |h| bounded by 1.1*2^26,1.1*2^25,1.1*2^26,1.1*2^25,etc.
*/

void spake2__fe_sub(spake2__fe_t h, const spake2__fe_t f, const spake2__fe_t g) {
    int32_t f0 = f[0];
    int32_t f1 = f[1];
    int32_t f2 = f[2];
    int32_t f3 = f[3];
    int32_t f4 = f[4];
    int32_t f5 = f[5];
    int32_t f6 = f[6];
    int32_t f7 = f[7];
    int32_t f8 = f[8];
    int32_t f9 = f[9];
    int32_t g0 = g[0];
    int32_t g1 = g[1];
    int32_t g2 = g[2];
    int32_t g3 = g[3];
    int32_t g4 = g[4];
    int32_t g5 = g[5];
    int32_t g6 = g[6];
    int32_t g7 = g[7];
    int32_t g8 = g[8];
    int32_t g9 = g[9];
    int32_t h0 = f0 - g0;
    int32_t h1 = f1 - g1;
    int32_t h2 = f2 - g2;
    int32_t h3 = f3 - g3;
    int32_t h4 = f4 - g4;
    int32_t h5 = f5 - g5;
    int32_t h6 = f6 - g6;
    int32_t h7 = f7 - g7;
    int32_t h8 = f8 - g8;
    int32_t h9 = f9 - g9;

    h[0] = h0;
    h[1] = h1;
    h[2] = h2;
    h[3] = h3;
    h[4] = h4;
    h[5] = h5;
    h[6] = h6;
    h[7] = h7;
    h[8] = h8;
    h[9] = h9;
}



/*
Preconditions:
  |h| bounded by 1.1*2^26,1.1*2^25,1.1*2^26,1.1*2^25,etc.

Write p=2^255-19; q=floor(h/p).
Basic claim: q = floor(2^(-255)(h + 19 2^(-25)h9 + 2^(-1))).

Proof:
  Have |h|<=p so |q|<=1 so |19^2 2^(-255) q|<1/4.
  Also have |h-2^230 h9|<2^231 so |19 2^(-255)(h-2^230 h9)|<1/4.

  Write y=2^(-1)-19^2 2^(-255)q-19 2^(-255)(h-2^230 h9).
  Then 0<y<1.

  Write r=h-pq.
  Have 0<=r<=p-1=2^255-20.
  Thus 0<=r+19(2^-255)r<r+19(2^-255)2^255<=2^255-1.

  Write x=r+19(2^-255)r+y.
  Then 0<x<2^255 so floor(2^(-255)x) = 0 so floor(q+2^(-255)x) = q.

  Have q+2^(-255)x = 2^(-255)(h + 19 2^(-25) h9 + 2^(-1))
  so floor(2^(-255)(h + 19 2^(-25) h9 + 2^(-1))) = q.
*/

void spake2__fe_tobytes(unsigned char *s, const spake2__fe_t h) {
    int32_t h0 = h[0];
    int32_t h1 = h[1];
    int32_t h2 = h[2];
    int32_t h3 = h[3];
    int32_t h4 = h[4];
    int32_t h5 = h[5];
    int32_t h6 = h[6];
    int32_t h7 = h[7];
    int32_t h8 = h[8];
    int32_t h9 = h[9];
    int32_t q;
    int32_t carry = 0;
    q = (19 * h9 + (((int32_t) 1) << 24)) >> 25;
    q = (h0 + q) >> 26;
    q = (h1 + q) >> 25;
    q = (h2 + q) >> 26;
    q = (h3 + q) >> 25;
    q = (h4 + q) >> 26;
    q = (h5 + q) >> 25;
    q = (h6 + q) >> 26;
    q = (h7 + q) >> 25;
    q = (h8 + q) >> 26;
    q = (h9 + q) >> 25;
    /* Goal: Output h-(2^255-19)q, which is between 0 and 2^255-20. */
    h0 += 19 * q;
    /* Goal: Output h-2^255 q, which is between 0 and 2^255-20. */
    carry = h0 >> 26; h1 += carry; h0 -= carry << 26;
    carry = h1 >> 25; h2 += carry; h1 -= carry << 25;
    carry = h2 >> 26; h3 += carry; h2 -= carry << 26;
    carry = h3 >> 25; h4 += carry; h3 -= carry << 25;
    carry = h4 >> 26; h5 += carry; h4 -= carry << 26;
    carry = h5 >> 25; h6 += carry; h5 -= carry << 25;
    carry = h6 >> 26; h7 += carry; h6 -= carry << 26;
    carry = h7 >> 25; h8 += carry; h7 -= carry << 25;
    carry = h8 >> 26; h9 += carry; h8 -= carry << 26; 
    carry = h9 >> 25; h9 -= carry << 25;

    /* h10 = carry */
    /*
    Goal: Output h0+...+2^255 h10-2^255 q, which is between 0 and 2^255-20.
    Have h0+...+2^230 h9 between 0 and 2^255-1;
    evidently 2^255 h10-2^255 q = 0.
    Goal: Output h0+...+2^230 h9.
    */
    s[0] = (unsigned char)(h0 >> 0);
    s[1] = (unsigned char)(h0 >> 8);
    s[2] = (unsigned char)(h0 >> 16);
    s[3] = (unsigned char)((h0 >> 24) | (h1 << 2));
    s[4] = (unsigned char)(h1 >> 6);
    s[5] = (unsigned char)(h1 >> 14);
    s[6] = (unsigned char)((h1 >> 22) | (h2 << 3));
    s[7] = (unsigned char)(h2 >> 5);
    s[8] = (unsigned char)(h2 >> 13);
    s[9] = (unsigned char)((h2 >> 21) | (h3 << 5));
    s[10] = (unsigned char)(h3 >> 3);
    s[11] = (unsigned char)(h3 >> 11);
    s[12] = (unsigned char)((h3 >> 19) | (h4 << 6));
    s[13] = (unsigned char)(h4 >> 2);
    s[14] = (unsigned char)(h4 >> 10);
    s[15] = (unsigned char)(h4 >> 18);
    s[16] = (unsigned char)(h5 >> 0);
    s[17] = (unsigned char)(h5 >> 8);
    s[18] = (unsigned char)(h5 >> 16);
    s[19] = (unsigned char)((h5 >> 24) | (h6 << 1));
    s[20] = (unsigned char)(h6 >> 7);
    s[21] = (unsigned char)(h6 >> 15);
    s[22] = (unsigned char)((h6 >> 23) | (h7 << 3));
    s[23] = (unsigned char)(h7 >> 5);
    s[24] = (unsigned char)(h7 >> 13);
    s[25] = (unsigned char)((h7 >> 21) | (h8 << 4));
    s[26] = (unsigned char)(h8 >> 4);
    s[27] = (unsigned char)(h8 >> 12);
    s[28] = (unsigned char)((h8 >> 20) | (h9 << 6));
    s[29] = (unsigned char)(h9 >> 2);
    s[30] = (unsigned char)(h9 >> 10);
    s[31] = (unsigned char)(h9 >> 18);
}
