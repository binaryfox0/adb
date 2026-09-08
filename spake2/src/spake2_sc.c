#include "spake2_sc.h"

static uint64_t load_3(const uint8_t in[3]) {
    uint64_t result;
    result = (uint64_t)in[0];
    result |= ((uint64_t)in[1]) << 8;
    result |= ((uint64_t)in[2]) << 16;
    return result;
}

static uint64_t load_4(const uint8_t in[4]) {
    uint64_t result;
    result = (uint64_t)in[0];
    result |= ((uint64_t)in[1]) << 8;
    result |= ((uint64_t)in[2]) << 16;
    result |= ((uint64_t)in[3]) << 24;
    return result;
}

// int64_lshift21 returns `a << 21` but is defined when shifting bits into the
// sign bit. This works around a language flaw in C.
static int64_t int64_lshift21(int64_t a) {
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
    int64_t s0 = 2097151 & load_3(arg1->v);
    int64_t s1 = 2097151 & (load_4(arg1->v + 2) >> 5);
    int64_t s2 = 2097151 & (load_3(arg1->v + 5) >> 2);
    int64_t s3 = 2097151 & (load_4(arg1->v + 7) >> 7);
    int64_t s4 = 2097151 & (load_4(arg1->v + 10) >> 4);
    int64_t s5 = 2097151 & (load_3(arg1->v + 13) >> 1);
    int64_t s6 = 2097151 & (load_4(arg1->v + 15) >> 6);
    int64_t s7 = 2097151 & (load_3(arg1->v + 18) >> 3);
    int64_t s8 = 2097151 & load_3(arg1->v + 21);
    int64_t s9 = 2097151 & (load_4(arg1->v + 23) >> 5);
    int64_t s10 = 2097151 & (load_3(arg1->v + 26) >> 2);
    int64_t s11 = 2097151 & (load_4(arg1->v + 28) >> 7);
    int64_t s12 = 2097151 & (load_4(arg1->v + 31) >> 4);
    int64_t s13 = 2097151 & (load_3(arg1->v + 34) >> 1);
    int64_t s14 = 2097151 & (load_4(arg1->v + 36) >> 6);
    int64_t s15 = 2097151 & (load_3(arg1->v + 39) >> 3);
    int64_t s16 = 2097151 & load_3(arg1->v + 42);
    int64_t s17 = 2097151 & (load_4(arg1->v + 44) >> 5);
    int64_t s18 = 2097151 & (load_3(arg1->v + 47) >> 2);
    int64_t s19 = 2097151 & (load_4(arg1->v + 49) >> 7);
    int64_t s20 = 2097151 & (load_4(arg1->v + 52) >> 4);
    int64_t s21 = 2097151 & (load_3(arg1->v + 55) >> 1);
    int64_t s22 = 2097151 & (load_4(arg1->v + 57) >> 6);
    int64_t s23 = (load_4(arg1->v + 60) >> 3);
    int64_t carry0;
    int64_t carry1;
    int64_t carry2;
    int64_t carry3;
    int64_t carry4;
    int64_t carry5;
    int64_t carry6;
    int64_t carry7;
    int64_t carry8;
    int64_t carry9;
    int64_t carry10;
    int64_t carry11;
    int64_t carry12;
    int64_t carry13;
    int64_t carry14;
    int64_t carry15;
    int64_t carry16;

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

    carry6 = (s6 + (1 << 20)) >> 21;
    s7 += carry6;
    s6 -= int64_lshift21(carry6);
    carry8 = (s8 + (1 << 20)) >> 21;
    s9 += carry8;
    s8 -= int64_lshift21(carry8);
    carry10 = (s10 + (1 << 20)) >> 21;
    s11 += carry10;
    s10 -= int64_lshift21(carry10);
    carry12 = (s12 + (1 << 20)) >> 21;
    s13 += carry12;
    s12 -= int64_lshift21(carry12);
    carry14 = (s14 + (1 << 20)) >> 21;
    s15 += carry14;
    s14 -= int64_lshift21(carry14);
    carry16 = (s16 + (1 << 20)) >> 21;
    s17 += carry16;
    s16 -= int64_lshift21(carry16);

    carry7 = (s7 + (1 << 20)) >> 21;
    s8 += carry7;
    s7 -= int64_lshift21(carry7);
    carry9 = (s9 + (1 << 20)) >> 21;
    s10 += carry9;
    s9 -= int64_lshift21(carry9);
    carry11 = (s11 + (1 << 20)) >> 21;
    s12 += carry11;
    s11 -= int64_lshift21(carry11);
    carry13 = (s13 + (1 << 20)) >> 21;
    s14 += carry13;
    s13 -= int64_lshift21(carry13);
    carry15 = (s15 + (1 << 20)) >> 21;
    s16 += carry15;
    s15 -= int64_lshift21(carry15);

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

    carry0 = (s0 + (1 << 20)) >> 21;
    s1 += carry0;
    s0 -= int64_lshift21(carry0);
    carry2 = (s2 + (1 << 20)) >> 21;
    s3 += carry2;
    s2 -= int64_lshift21(carry2);
    carry4 = (s4 + (1 << 20)) >> 21;
    s5 += carry4;
    s4 -= int64_lshift21(carry4);
    carry6 = (s6 + (1 << 20)) >> 21;
    s7 += carry6;
    s6 -= int64_lshift21(carry6);
    carry8 = (s8 + (1 << 20)) >> 21;
    s9 += carry8;
    s8 -= int64_lshift21(carry8);
    carry10 = (s10 + (1 << 20)) >> 21;
    s11 += carry10;
    s10 -= int64_lshift21(carry10);

    carry1 = (s1 + (1 << 20)) >> 21;
    s2 += carry1;
    s1 -= int64_lshift21(carry1);
    carry3 = (s3 + (1 << 20)) >> 21;
    s4 += carry3;
    s3 -= int64_lshift21(carry3);
    carry5 = (s5 + (1 << 20)) >> 21;
    s6 += carry5;
    s5 -= int64_lshift21(carry5);
    carry7 = (s7 + (1 << 20)) >> 21;
    s8 += carry7;
    s7 -= int64_lshift21(carry7);
    carry9 = (s9 + (1 << 20)) >> 21;
    s10 += carry9;
    s9 -= int64_lshift21(carry9);
    carry11 = (s11 + (1 << 20)) >> 21;
    s12 += carry11;
    s11 -= int64_lshift21(carry11);

    s0 += s12 * 666643;
    s1 += s12 * 470296;
    s2 += s12 * 654183;
    s3 -= s12 * 997805;
    s4 += s12 * 136657;
    s5 -= s12 * 683901;
    s12 = 0;

    carry0 = s0 >> 21;
    s1 += carry0;
    s0 -= int64_lshift21(carry0);
    carry1 = s1 >> 21;
    s2 += carry1;
    s1 -= int64_lshift21(carry1);
    carry2 = s2 >> 21;
    s3 += carry2;
    s2 -= int64_lshift21(carry2);
    carry3 = s3 >> 21;
    s4 += carry3;
    s3 -= int64_lshift21(carry3);
    carry4 = s4 >> 21;
    s5 += carry4;
    s4 -= int64_lshift21(carry4);
    carry5 = s5 >> 21;
    s6 += carry5;
    s5 -= int64_lshift21(carry5);
    carry6 = s6 >> 21;
    s7 += carry6;
    s6 -= int64_lshift21(carry6);
    carry7 = s7 >> 21;
    s8 += carry7;
    s7 -= int64_lshift21(carry7);
    carry8 = s8 >> 21;
    s9 += carry8;
    s8 -= int64_lshift21(carry8);
    carry9 = s9 >> 21;
    s10 += carry9;
    s9 -= int64_lshift21(carry9);
    carry10 = s10 >> 21;
    s11 += carry10;
    s10 -= int64_lshift21(carry10);
    carry11 = s11 >> 21;
    s12 += carry11;
    s11 -= int64_lshift21(carry11);

    s0 += s12 * 666643;
    s1 += s12 * 470296;
    s2 += s12 * 654183;
    s3 -= s12 * 997805;
    s4 += s12 * 136657;
    s5 -= s12 * 683901;
    s12 = 0;

    carry0 = s0 >> 21;
    s1 += carry0;
    s0 -= int64_lshift21(carry0);
    carry1 = s1 >> 21;
    s2 += carry1;
    s1 -= int64_lshift21(carry1);
    carry2 = s2 >> 21;
    s3 += carry2;
    s2 -= int64_lshift21(carry2);
    carry3 = s3 >> 21;
    s4 += carry3;
    s3 -= int64_lshift21(carry3);
    carry4 = s4 >> 21;
    s5 += carry4;
    s4 -= int64_lshift21(carry4);
    carry5 = s5 >> 21;
    s6 += carry5;
    s5 -= int64_lshift21(carry5);
    carry6 = s6 >> 21;
    s7 += carry6;
    s6 -= int64_lshift21(carry6);
    carry7 = s7 >> 21;
    s8 += carry7;
    s7 -= int64_lshift21(carry7);
    carry8 = s8 >> 21;
    s9 += carry8;
    s8 -= int64_lshift21(carry8);
    carry9 = s9 >> 21;
    s10 += carry9;
    s9 -= int64_lshift21(carry9);
    carry10 = s10 >> 21;
    s11 += carry10;
    s10 -= int64_lshift21(carry10);

    out1->v[0] = s0 >> 0;
    out1->v[1] = s0 >> 8;
    out1->v[2] = (s0 >> 16) | (s1 << 5);
    out1->v[3] = s1 >> 3;
    out1->v[4] = s1 >> 11;
    out1->v[5] = (s1 >> 19) | (s2 << 2);
    out1->v[6] = s2 >> 6;
    out1->v[7] = (s2 >> 14) | (s3 << 7);
    out1->v[8] = s3 >> 1;
    out1->v[9] = s3 >> 9;
    out1->v[10] = (s3 >> 17) | (s4 << 4);
    out1->v[11] = s4 >> 4;
    out1->v[12] = s4 >> 12;
    out1->v[13] = (s4 >> 20) | (s5 << 1);
    out1->v[14] = s5 >> 7;
    out1->v[15] = (s5 >> 15) | (s6 << 6);
    out1->v[16] = s6 >> 2;
    out1->v[17] = s6 >> 10;
    out1->v[18] = (s6 >> 18) | (s7 << 3);
    out1->v[19] = s7 >> 5;
    out1->v[20] = s7 >> 13;
    out1->v[21] = s8 >> 0;
    out1->v[22] = s8 >> 8;
    out1->v[23] = (s8 >> 16) | (s9 << 5);
    out1->v[24] = s9 >> 3;
    out1->v[25] = s9 >> 11;
    out1->v[26] = (s9 >> 19) | (s10 << 2);
    out1->v[27] = s10 >> 6;
    out1->v[28] = (s10 >> 14) | (s11 << 7);
    out1->v[29] = s11 >> 1;
    out1->v[30] = s11 >> 9;
    out1->v[31] = s11 >> 17;
}

void spake2__sc_lshift3(
        spake2__sc_t *out1)
{
    uint8_t carry = 0;
    for (unsigned i = 0; i < 32; i++) 
    {
        const uint8_t next_carry = out1->v[i] >> 5;
        out1->v[i] = (out1->v[i] << 3) | carry;
        carry = next_carry;
    }
}
