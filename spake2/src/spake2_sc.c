#include "spake2_sc.h"

static uint64_t spake2__load3(
        const uint8_t in[3]) 
{
    uint64_t result;
    result = (uint64_t)in[0];
    result |= ((uint64_t)in[1]) << 8;
    result |= ((uint64_t)in[2]) << 16;
    return result;
}

static uint64_t spake2__load4(
        const uint8_t in[4]) 
{
    uint64_t result;
    result = (uint64_t)in[0];
    result |= ((uint64_t)in[1]) << 8;
    result |= ((uint64_t)in[2]) << 16;
    result |= ((uint64_t)in[3]) << 24;
    return result;
}

// int64_lshift21 returns `a << 21` but is defined when shifting bits into the
// sign bit. This works around a language flaw in C.
static int64_t spake2__int64_lshift21(int64_t a) {
    return (int64_t)((uint64_t)a << 21);
}
// The set of scalars is \Z/l
// where l = 2^252 + 27742317777372353535851937790883648493.

// Input:
//   s[0]+256*s[1]+...+256^63*s[63] = s
//
// Output:
//   s[0]+256*s[1]+...+256^31*s[31] = s mod l
//   where l = 2^252 + 27742317777372353535851937790883648493.
//   Overwrites s in place.
void spake2__sc_reduce(
        spake2__sc_t *out1,
        const spake2__sc_wide_t *arg1)
{
    int64_t s0 = 2097151 & spake2__load3(arg1->v);
    int64_t s1 = 2097151 & (spake2__load4(arg1->v + 2) >> 5);
    int64_t s2 = 2097151 & (spake2__load3(arg1->v + 5) >> 2);
    int64_t s3 = 2097151 & (spake2__load4(arg1->v + 7) >> 7);
    int64_t s4 = 2097151 & (spake2__load4(arg1->v + 10) >> 4);
    int64_t s5 = 2097151 & (spake2__load3(arg1->v + 13) >> 1);
    int64_t s6 = 2097151 & (spake2__load4(arg1->v + 15) >> 6);
    int64_t s7 = 2097151 & (spake2__load3(arg1->v + 18) >> 3);
    int64_t s8 = 2097151 & spake2__load3(arg1->v + 21);
    int64_t s9 = 2097151 & (spake2__load4(arg1->v + 23) >> 5);
    int64_t s10 = 2097151 & (spake2__load3(arg1->v + 26) >> 2);
    int64_t s11 = 2097151 & (spake2__load4(arg1->v + 28) >> 7);
    int64_t s12 = 2097151 & (spake2__load4(arg1->v + 31) >> 4);
    int64_t s13 = 2097151 & (spake2__load3(arg1->v + 34) >> 1);
    int64_t s14 = 2097151 & (spake2__load4(arg1->v + 36) >> 6);
    int64_t s15 = 2097151 & (spake2__load3(arg1->v + 39) >> 3);
    int64_t s16 = 2097151 & spake2__load3(arg1->v + 42);
    int64_t s17 = 2097151 & (spake2__load4(arg1->v + 44) >> 5);
    int64_t s18 = 2097151 & (spake2__load3(arg1->v + 47) >> 2);
    int64_t s19 = 2097151 & (spake2__load4(arg1->v + 49) >> 7);
    int64_t s20 = 2097151 & (spake2__load4(arg1->v + 52) >> 4);
    int64_t s21 = 2097151 & (spake2__load3(arg1->v + 55) >> 1);
    int64_t s22 = 2097151 & (spake2__load4(arg1->v + 57) >> 6);
    int64_t s23 = (spake2__load4(arg1->v + 60) >> 3);
    int64_t carry = 0;

    s11 += s23 * 666643;
    s12 += s23 * 470296;
    s13 += s23 * 654183;
    s14 -= s23 * 997805;
    s15 += s23 * 136657;
    s16 -= s23 * 683901;
    s23 = 0;

    s10 += s22 * 666643;
    s11 += s22 * 470296;
    s12 += s22 * 654183;
    s13 -= s22 * 997805;
    s14 += s22 * 136657;
    s15 -= s22 * 683901;
    s22 = 0;

    s9 += s21 * 666643;
    s10 += s21 * 470296;
    s11 += s21 * 654183;
    s12 -= s21 * 997805;
    s13 += s21 * 136657;
    s14 -= s21 * 683901;
    s21 = 0;

    s8 += s20 * 666643;
    s9 += s20 * 470296;
    s10 += s20 * 654183;
    s11 -= s20 * 997805;
    s12 += s20 * 136657;
    s13 -= s20 * 683901;
    s20 = 0;

    s7 += s19 * 666643;
    s8 += s19 * 470296;
    s9 += s19 * 654183;
    s10 -= s19 * 997805;
    s11 += s19 * 136657;
    s12 -= s19 * 683901;
    s19 = 0;

    s6 += s18 * 666643;
    s7 += s18 * 470296;
    s8 += s18 * 654183;
    s9 -= s18 * 997805;
    s10 += s18 * 136657;
    s11 -= s18 * 683901;
    s18 = 0;

    carry = (s6 + (1 << 20)) >> 21;
    s7 += carry;
    s6 -= spake2__int64_lshift21(carry);
    carry = (s8 + (1 << 20)) >> 21;
    s9 += carry;
    s8 -= spake2__int64_lshift21(carry);
    carry = (s10 + (1 << 20)) >> 21;
    s11 += carry;
    s10 -= spake2__int64_lshift21(carry);
    carry = (s12 + (1 << 20)) >> 21;
    s13 += carry;
    s12 -= spake2__int64_lshift21(carry);
    carry = (s14 + (1 << 20)) >> 21;
    s15 += carry;
    s14 -= spake2__int64_lshift21(carry);
    carry = (s16 + (1 << 20)) >> 21;
    s17 += carry;
    s16 -= spake2__int64_lshift21(carry);

    carry = (s7 + (1 << 20)) >> 21;
    s8 += carry;
    s7 -= spake2__int64_lshift21(carry);
    carry = (s9 + (1 << 20)) >> 21;
    s10 += carry;
    s9 -= spake2__int64_lshift21(carry);
    carry = (s11 + (1 << 20)) >> 21;
    s12 += carry;
    s11 -= spake2__int64_lshift21(carry);
    carry = (s13 + (1 << 20)) >> 21;
    s14 += carry;
    s13 -= spake2__int64_lshift21(carry);
    carry = (s15 + (1 << 20)) >> 21;
    s16 += carry;
    s15 -= spake2__int64_lshift21(carry);

    s5 += s17 * 666643;
    s6 += s17 * 470296;
    s7 += s17 * 654183;
    s8 -= s17 * 997805;
    s9 += s17 * 136657;
    s10 -= s17 * 683901;
    s17 = 0;

    s4 += s16 * 666643;
    s5 += s16 * 470296;
    s6 += s16 * 654183;
    s7 -= s16 * 997805;
    s8 += s16 * 136657;
    s9 -= s16 * 683901;
    s16 = 0;

    s3 += s15 * 666643;
    s4 += s15 * 470296;
    s5 += s15 * 654183;
    s6 -= s15 * 997805;
    s7 += s15 * 136657;
    s8 -= s15 * 683901;
    s15 = 0;

    s2 += s14 * 666643;
    s3 += s14 * 470296;
    s4 += s14 * 654183;
    s5 -= s14 * 997805;
    s6 += s14 * 136657;
    s7 -= s14 * 683901;
    s14 = 0;

    s1 += s13 * 666643;
    s2 += s13 * 470296;
    s3 += s13 * 654183;
    s4 -= s13 * 997805;
    s5 += s13 * 136657;
    s6 -= s13 * 683901;
    s13 = 0;

    s0 += s12 * 666643;
    s1 += s12 * 470296;
    s2 += s12 * 654183;
    s3 -= s12 * 997805;
    s4 += s12 * 136657;
    s5 -= s12 * 683901;
    s12 = 0;

    carry = (s0 + (1 << 20)) >> 21;
    s1 += carry;
    s0 -= spake2__int64_lshift21(carry);
    carry = (s2 + (1 << 20)) >> 21;
    s3 += carry;
    s2 -= spake2__int64_lshift21(carry);
    carry = (s4 + (1 << 20)) >> 21;
    s5 += carry;
    s4 -= spake2__int64_lshift21(carry);
    carry = (s6 + (1 << 20)) >> 21;
    s7 += carry;
    s6 -= spake2__int64_lshift21(carry);
    carry = (s8 + (1 << 20)) >> 21;
    s9 += carry;
    s8 -= spake2__int64_lshift21(carry);
    carry = (s10 + (1 << 20)) >> 21;
    s11 += carry;
    s10 -= spake2__int64_lshift21(carry);

    carry = (s1 + (1 << 20)) >> 21;
    s2 += carry;
    s1 -= spake2__int64_lshift21(carry);
    carry = (s3 + (1 << 20)) >> 21;
    s4 += carry;
    s3 -= spake2__int64_lshift21(carry);
    carry = (s5 + (1 << 20)) >> 21;
    s6 += carry;
    s5 -= spake2__int64_lshift21(carry);
    carry = (s7 + (1 << 20)) >> 21;
    s8 += carry;
    s7 -= spake2__int64_lshift21(carry);
    carry = (s9 + (1 << 20)) >> 21;
    s10 += carry;
    s9 -= spake2__int64_lshift21(carry);
    carry = (s11 + (1 << 20)) >> 21;
    s12 += carry;
    s11 -= spake2__int64_lshift21(carry);

    s0 += s12 * 666643;
    s1 += s12 * 470296;
    s2 += s12 * 654183;
    s3 -= s12 * 997805;
    s4 += s12 * 136657;
    s5 -= s12 * 683901;
    s12 = 0;

    carry = s0 >> 21;
    s1 += carry;
    s0 -= spake2__int64_lshift21(carry);
    carry = s1 >> 21;
    s2 += carry;
    s1 -= spake2__int64_lshift21(carry);
    carry = s2 >> 21;
    s3 += carry;
    s2 -= spake2__int64_lshift21(carry);
    carry = s3 >> 21;
    s4 += carry;
    s3 -= spake2__int64_lshift21(carry);
    carry = s4 >> 21;
    s5 += carry;
    s4 -= spake2__int64_lshift21(carry);
    carry = s5 >> 21;
    s6 += carry;
    s5 -= spake2__int64_lshift21(carry);
    carry = s6 >> 21;
    s7 += carry;
    s6 -= spake2__int64_lshift21(carry);
    carry = s7 >> 21;
    s8 += carry;
    s7 -= spake2__int64_lshift21(carry);
    carry = s8 >> 21;
    s9 += carry;
    s8 -= spake2__int64_lshift21(carry);
    carry = s9 >> 21;
    s10 += carry;
    s9 -= spake2__int64_lshift21(carry);
    carry = s10 >> 21;
    s11 += carry;
    s10 -= spake2__int64_lshift21(carry);
    carry = s11 >> 21;
    s12 += carry;
    s11 -= spake2__int64_lshift21(carry);

    s0 += s12 * 666643;
    s1 += s12 * 470296;
    s2 += s12 * 654183;
    s3 -= s12 * 997805;
    s4 += s12 * 136657;
    s5 -= s12 * 683901;
    s12 = 0;

    carry = s0 >> 21;
    s1 += carry;
    s0 -= spake2__int64_lshift21(carry);
    carry = s1 >> 21;
    s2 += carry;
    s1 -= spake2__int64_lshift21(carry);
    carry = s2 >> 21;
    s3 += carry;
    s2 -= spake2__int64_lshift21(carry);
    carry = s3 >> 21;
    s4 += carry;
    s3 -= spake2__int64_lshift21(carry);
    carry = s4 >> 21;
    s5 += carry;
    s4 -= spake2__int64_lshift21(carry);
    carry = s5 >> 21;
    s6 += carry;
    s5 -= spake2__int64_lshift21(carry);
    carry = s6 >> 21;
    s7 += carry;
    s6 -= spake2__int64_lshift21(carry);
    carry = s7 >> 21;
    s8 += carry;
    s7 -= spake2__int64_lshift21(carry);
    carry = s8 >> 21;
    s9 += carry;
    s8 -= spake2__int64_lshift21(carry);
    carry = s9 >> 21;
    s10 += carry;
    s9 -= spake2__int64_lshift21(carry);
    carry = s10 >> 21;
    s11 += carry;
    s10 -= spake2__int64_lshift21(carry);

    out1->v[0] = (uint8_t)(s0 >> 0);
    out1->v[1] = (uint8_t)(s0 >> 8);
    out1->v[2] = (uint8_t)((s0 >> 16) | (s1 << 5));
    out1->v[3] = (uint8_t)(s1 >> 3);
    out1->v[4] = (uint8_t)(s1 >> 11);
    out1->v[5] = (uint8_t)((s1 >> 19) | (s2 << 2));
    out1->v[6] = (uint8_t)(s2 >> 6);
    out1->v[7] = (uint8_t)((s2 >> 14) | (s3 << 7));
    out1->v[8] = (uint8_t)(s3 >> 1);
    out1->v[9] = (uint8_t)(s3 >> 9);
    out1->v[10] = (uint8_t)((s3 >> 17) | (s4 << 4));
    out1->v[11] = (uint8_t)(s4 >> 4);
    out1->v[12] = (uint8_t)(s4 >> 12);
    out1->v[13] = (uint8_t)((s4 >> 20) | (s5 << 1));
    out1->v[14] = (uint8_t)(s5 >> 7);
    out1->v[15] = (uint8_t)((s5 >> 15) | (s6 << 6));
    out1->v[16] = (uint8_t)(s6 >> 2);
    out1->v[17] = (uint8_t)(s6 >> 10);
    out1->v[18] = (uint8_t)((s6 >> 18) | (s7 << 3));
    out1->v[19] = (uint8_t)(s7 >> 5);
    out1->v[20] = (uint8_t)(s7 >> 13);
    out1->v[21] = (uint8_t)(s8 >> 0);
    out1->v[22] = (uint8_t)(s8 >> 8);
    out1->v[23] = (uint8_t)((s8 >> 16) | (s9 << 5));
    out1->v[24] = (uint8_t)(s9 >> 3);
    out1->v[25] = (uint8_t)(s9 >> 11);
    out1->v[26] = (uint8_t)((s9 >> 19) | (s10 << 2));
    out1->v[27] = (uint8_t)(s10 >> 6);
    out1->v[28] = (uint8_t)((s10 >> 14) | (s11 << 7));
    out1->v[29] = (uint8_t)(s11 >> 1);
    out1->v[30] = (uint8_t)(s11 >> 9);
    out1->v[31] = (uint8_t)(s11 >> 17);
}

void spake2__sc_lshift3(
        spake2__sc_t *out1)
{
    uint8_t carry = 0;
    for (unsigned i = 0; i < 32; i++) 
    {
        const uint8_t next_carry = out1->v[i] >> 5;
        out1->v[i] = (uint8_t)((out1->v[i] << 3) | carry);
        carry = next_carry;
    }
}
