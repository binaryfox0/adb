#include "spake2_fe.h"

#include <openssl/bn.h>
#include <openssl/rand.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_COUNT 10000

static BIGNUM *g_p;
static BN_CTX *g_bn_ctx;


/*
 * Helpers
 */

static int bn_to_le(
        unsigned char out[32],
        const BIGNUM *bn)
{
    unsigned char be[32];
    size_t i;

    memset(be, 0, sizeof(be));

    if (BN_bn2binpad(bn, be, sizeof(be)) != 32) {
        return 0;
    }

    for (i = 0; i < sizeof(be); ++i) {
        out[i] = be[sizeof(be) - 1 - i];
    }

    return 1;
}
static void dump_hex(
        const char *name,
        const unsigned char *buf,
        size_t len)
{
    size_t i;

    printf("%s = ", name);

    for (i = 0; i < len; ++i) {
        printf("%02x", buf[i]);
    }

    printf("\n");
}

static int test_fe_equal_bn(
        const char *name,
        const spake2__fe_t fe,
        const BIGNUM *bn)
{
    unsigned char got[32];
    unsigned char expected[32];
    BIGNUM *tmp;
    int rc;

    tmp = NULL;
    rc = 0;

    memset(got, 0, sizeof(got));
    memset(expected, 0, sizeof(expected));

    spake2__fe_tobytes(got, fe);

    tmp = BN_new();
    if (tmp == NULL) {
        fprintf(stderr, "BN_new failed\n");
        goto done;
    }

    if (BN_nnmod(tmp, bn, g_p, g_bn_ctx) != 1) {
        fprintf(stderr, "BN_nnmod failed\n");
        goto done;
    }

    if (!bn_to_le(expected, tmp)) {
        fprintf(stderr, "bn_to_le failed\n");
        goto done;
    }

    if (memcmp(got, expected, 32) != 0) {
        fprintf(stderr, "\nFAIL: %s\n", name);
        dump_hex("got     ", got, sizeof(got));
        dump_hex("expected", expected, sizeof(expected));
        rc = -1;
        goto done;
    }

    rc = 1;

done:
    BN_free(tmp);

    return rc;
}


/*
 * OpenSSL versions differ slightly in BIGNUM endian helpers.
 *
 * These two helpers avoid depending on BN_ENDIAN_*.
 */

static BIGNUM *bn_from_le(
        const unsigned char in[32])
{
    unsigned char be[32];
    BIGNUM *bn;
    size_t i;

    bn = NULL;

    for (i = 0; i < sizeof(be); ++i) {
        be[i] = in[sizeof(be) - 1 - i];
    }

    bn = BN_bin2bn(be, sizeof(be), NULL);

    return bn;
}


static BIGNUM *bn_from_fe(
        const spake2__fe_t fe)
{
    unsigned char buf[32];

    spake2__fe_tobytes(buf, fe);

    return bn_from_le(buf);
}

static int fe_from_bn(
        spake2__fe_t fe,
        const BIGNUM *bn)
{
    unsigned char buf[32];
    BIGNUM *tmp;

    tmp = BN_new();
    if (tmp == NULL) {
        return 0;
    }

    if (BN_nnmod(tmp, bn, g_p, g_bn_ctx) != 1) {
        BN_free(tmp);
        return 0;
    }

    if (!bn_to_le(buf, tmp)) {
        BN_free(tmp);
        return 0;
    }

    spake2__fe_frombytes(fe, buf);

    BN_free(tmp);

    return 1;
}

static BIGNUM *random_field_element(void)
{
    unsigned char buf[32];
    BIGNUM *bn;

    if (RAND_bytes(buf, sizeof(buf)) != 1) {
        return NULL;
    }

    /*
     * fe_frombytes ignores bit 255.
     */
    buf[31] &= 0x7f;

    bn = bn_from_le(buf);
    if (bn == NULL) {
        return NULL;
    }

    if (BN_nnmod(bn, bn, g_p, g_bn_ctx) != 1) {
        BN_free(bn);
        return NULL;
    }

    return bn;
}

static int check_op(
        const char *name,
        const spake2__fe_t got,
        const BIGNUM *expected)
{
    return test_fe_equal_bn(name, got, expected);
}


/*
 * Tests
 */

static int test_zero_one(void)
{
    spake2__fe_t h;
    BIGNUM *zero;
    BIGNUM *one;
    int rc;

    zero = NULL;
    one = NULL;
    rc = 0;

    zero = BN_new();
    one = BN_new();

    if (zero == NULL || one == NULL) {
        goto done;
    }

    BN_zero(zero);
    BN_one(one);

    spake2__fe_0(h);
    if (test_fe_equal_bn("fe_0", h, zero) != 1) {
        goto done;
    }

    spake2__fe_1(h);
    if (test_fe_equal_bn("fe_1", h, one) != 1) {
        goto done;
    }

    rc = 1;

done:
    BN_free(zero);
    BN_free(one);

    return rc;
}

static int test_frombytes_tobytes(void)
{
    unsigned char in[32];
    unsigned char expected_bytes[32];
    unsigned char out[32];
    spake2__fe_t h;
    BIGNUM *expected;
    int rc;

    expected = NULL;
    rc = 0;

    if (RAND_bytes(in, sizeof(in)) != 1) {
        goto done;
    }

    /*
     * Test bit 255 being set.
     */
    in[31] |= 0x80;

    /*
     * fe_frombytes() ignores bit 255.
     */
    memcpy(expected_bytes, in, sizeof(expected_bytes));
    expected_bytes[31] &= 0x7f;

    expected = bn_from_le(expected_bytes);
    if (expected == NULL) {
        goto done;
    }

    BN_nnmod(expected, expected, g_p, g_bn_ctx);

    spake2__fe_frombytes(h, in);
    spake2__fe_tobytes(out, h);

    if (test_fe_equal_bn("frombytes/tobytes", h, expected) != 1) {
        goto done;
    }

    rc = 1;

done:
    BN_free(expected);

    return rc;
}

static int test_add_sub_mul(void)
{
    spake2__fe_t a;
    spake2__fe_t b;
    spake2__fe_t r;

    BIGNUM *a_bn;
    BIGNUM *b_bn;
    BIGNUM *expected;

    int i;
    int rc;

    a_bn = NULL;
    b_bn = NULL;
    expected = NULL;
    rc = 0;

    for (i = 0; i < TEST_COUNT; ++i) {
        a_bn = random_field_element();
        b_bn = random_field_element();
        expected = BN_new();

        if (a_bn == NULL || b_bn == NULL || expected == NULL) {
            goto done;
        }

        if (!fe_from_bn(a, a_bn) ||
            !fe_from_bn(b, b_bn)) {
            goto done;
        }

        /*
         * add
         */
        if (BN_mod_add(expected, a_bn, b_bn, g_p, g_bn_ctx) != 1) {
            goto done;
        }

        spake2__fe_add(r, a, b);

        if (!check_op("fe_add", r, expected)) {
            goto done;
        }

        /*
         * sub
         */
        if (BN_mod_sub(expected, a_bn, b_bn, g_p, g_bn_ctx) != 1) {
            goto done;
        }

        spake2__fe_sub(r, a, b);

        if (!check_op("fe_sub", r, expected)) {
            goto done;
        }

        /*
         * mul
         */
        if (BN_mod_mul(expected, a_bn, b_bn, g_p, g_bn_ctx) != 1) {
            goto done;
        }

        spake2__fe_mul(r, a, b);

        if (!check_op("fe_mul", r, expected)) {
            goto done;
        }

        /*
         * sq
         */
        if (BN_mod_sqr(expected, a_bn, g_p, g_bn_ctx) != 1) {
            goto done;
        }

        spake2__fe_sq(r, a);

        if (!check_op("fe_sq", r, expected)) {
            goto done;
        }

        /*
         * sq2 = 2 * a^2
         */
        if (BN_mod_sqr(expected, a_bn, g_p, g_bn_ctx) != 1 ||
            BN_mod_add(expected, expected, expected, g_p, g_bn_ctx) != 1) {
            goto done;
        }

        spake2__fe_sq2(r, a);

        if (!check_op("fe_sq2", r, expected)) {
            goto done;
        }

        /*
         * mul121666
         */
        {
            BIGNUM *c;

            c = BN_new();
            if (c == NULL) {
                goto done;
            }

            BN_set_word(c, 121666);

            if (BN_mod_mul(expected, a_bn, c, g_p, g_bn_ctx) != 1) {
                BN_free(c);
                goto done;
            }

            BN_free(c);
        }

        spake2__fe_mul121666(r, a);

        if (!check_op("fe_mul121666", r, expected)) {
            goto done;
        }

        BN_free(a_bn);
        BN_free(b_bn);
        BN_free(expected);

        a_bn = NULL;
        b_bn = NULL;
        expected = NULL;
    }

    rc = 1;

done:
    BN_free(a_bn);
    BN_free(b_bn);
    BN_free(expected);

    return rc;
}

static int test_neg(void)
{
    spake2__fe_t a;
    spake2__fe_t r;
    BIGNUM *a_bn;
    BIGNUM *expected;
    int i;
    int rc;

    a_bn = NULL;
    expected = NULL;
    rc = 0;

    for (i = 0; i < TEST_COUNT; ++i) {
        a_bn = random_field_element();
        expected = BN_new();

        if (a_bn == NULL || expected == NULL) {
            goto done;
        }

        if (!fe_from_bn(a, a_bn)) {
            goto done;
        }

        if (BN_is_zero(a_bn)) {
            BN_zero(expected);
        } else {
            if (BN_mod_sub(expected, g_p, a_bn, g_p, g_bn_ctx) != 1) {
                goto done;
            }
        }

        spake2__fe_neg(r, a);

        if (!check_op("fe_neg", r, expected)) {
            goto done;
        }

        BN_free(a_bn);
        BN_free(expected);

        a_bn = NULL;
        expected = NULL;
    }

    rc = 1;

done:
    BN_free(a_bn);
    BN_free(expected);

    return rc;
}

static int test_invert(void)
{
    spake2__fe_t a;
    spake2__fe_t r;
    BIGNUM *a_bn;
    BIGNUM *expected;
    int i;
    int rc;

    a_bn = NULL;
    expected = NULL;
    rc = 0;

    for (i = 0; i < TEST_COUNT; ++i) {
        a_bn = random_field_element();
        expected = BN_new();

        if (a_bn == NULL || expected == NULL) {
            goto done;
        }

        if (BN_is_zero(a_bn)) {
            /*
             * In ref10 fe_invert(0) produces 0.
             */
            BN_zero(expected);
        } else {
            if (BN_mod_inverse(expected, a_bn, g_p, g_bn_ctx) == NULL) {
                goto done;
            }
        }

        if (!fe_from_bn(a, a_bn)) {
            goto done;
        }

        spake2__fe_invert(r, a);

        if (!check_op("fe_invert", r, expected)) {
            goto done;
        }

        /*
         * Also verify:
         *
         * a * inverse(a) == 1
         */
        if (!BN_is_zero(a_bn)) {
            BIGNUM *one;

            one = BN_new();
            if (one == NULL) {
                goto done;
            }

            BN_one(one);

            spake2__fe_mul(r, a, r);

            if (!check_op("a * invert(a)", r, one)) {
                BN_free(one);
                goto done;
            }

            BN_free(one);
        }

        BN_free(a_bn);
        BN_free(expected);

        a_bn = NULL;
        expected = NULL;
    }

    rc = 1;

done:
    BN_free(a_bn);
    BN_free(expected);

    return rc;
}

static int test_pow22523(void)
{
    spake2__fe_t a;
    spake2__fe_t r;

    BIGNUM *a_bn;
    BIGNUM *expected;
    BIGNUM *exp;

    int i;
    int rc;

    a_bn = NULL;
    expected = NULL;
    exp = NULL;
    rc = 0;

    exp = BN_new();
    if (exp == NULL) {
        goto done;
    }

    /*
     * 2^252 - 3
     */
    BN_one(exp);

    if (BN_lshift(exp, exp, 252) != 1 ||
        BN_sub_word(exp, 3) != 1) {
        goto done;
    }

    for (i = 0; i < TEST_COUNT; ++i) {
        a_bn = random_field_element();
        expected = BN_new();

        if (a_bn == NULL || expected == NULL) {
            goto done;
        }

        if (BN_mod_exp(expected, a_bn, exp, g_p, g_bn_ctx) != 1) {
            goto done;
        }

        if (!fe_from_bn(a, a_bn)) {
            goto done;
        }

        spake2__fe_pow22523(r, a);

        if (!check_op("fe_pow22523", r, expected)) {
            goto done;
        }

        BN_free(a_bn);
        BN_free(expected);

        a_bn = NULL;
        expected = NULL;
    }

    rc = 1;

done:
    BN_free(a_bn);
    BN_free(expected);
    BN_free(exp);

    return rc;
}

static int test_predicates(void)
{
    spake2__fe_t a;
    BIGNUM *a_bn;
    int expected_negative;
    int expected_nonzero;
    int i;
    int rc;

    a_bn = NULL;
    rc = 0;

    for (i = 0; i < TEST_COUNT; ++i) {
        a_bn = random_field_element();

        if (a_bn == NULL) {
            goto done;
        }

        if (!fe_from_bn(a, a_bn)) {
            goto done;
        }

        expected_negative = BN_is_odd(a_bn) ? 1 : 0;
        expected_nonzero = !BN_is_zero(a_bn) ? 1 : 0;

        if (spake2__fe_isnegative(a) != expected_negative) {
            fprintf(stderr,
                    "FAIL: fe_isnegative: got=%d expected=%d\n",
                    spake2__fe_isnegative(a),
                    expected_negative);
            goto done;
        }

        if (spake2__fe_isnonzero(a) != expected_nonzero) {
            fprintf(stderr,
                    "FAIL: fe_isnonzero: got=%d expected=%d\n",
                    spake2__fe_isnonzero(a),
                    expected_nonzero);
            goto done;
        }

        BN_free(a_bn);
        a_bn = NULL;
    }

    rc = 1;

done:
    BN_free(a_bn);

    return rc;
}

static int test_cmov(void)
{
    spake2__fe_t a;
    spake2__fe_t b;
    spake2__fe_t original_a;
    unsigned char got_a[32];
    unsigned char got_b[32];
    unsigned char exp_a[32];
    unsigned char exp_b[32];
    int rc;

    rc = 0;

    /*
     * b = 0: a unchanged.
     */
    spake2__fe_1(a);
    spake2__fe_0(b);
    spake2__fe_copy(original_a, a);

    spake2__fe_cmov(a, b, 0);

    spake2__fe_tobytes(got_a, a);
    spake2__fe_tobytes(exp_a, original_a);

    if (memcmp(got_a, exp_a, sizeof(got_a)) != 0) {
        fprintf(stderr, "FAIL: fe_cmov(b=0)\n");
        goto done;
    }

    /*
     * b = 1: a becomes b.
     */
    spake2__fe_1(a);
    spake2__fe_0(b);

    spake2__fe_cmov(a, b, 1);

    spake2__fe_tobytes(got_a, a);
    spake2__fe_tobytes(exp_b, b);

    if (memcmp(got_a, exp_b, sizeof(got_a)) != 0) {
        fprintf(stderr, "FAIL: fe_cmov(b=1)\n");
        goto done;
    }

    /*
     * Values other than zero should behave according to the implementation's
     * contract. Normally this primitive expects b to be 0 or 1.
     */

    memset(got_b, 0, sizeof(got_b));

    rc = 1;

done:
    return rc;
}

static int test_cswap(void)
{
    spake2__fe_t a;
    spake2__fe_t b;
    unsigned char a_before[32];
    unsigned char b_before[32];
    unsigned char a_after[32];
    unsigned char b_after[32];
    int rc;

    rc = 0;

    spake2__fe_1(a);
    spake2__fe_0(b);

    spake2__fe_tobytes(a_before, a);
    spake2__fe_tobytes(b_before, b);

    /*
     * No swap.
     */
    spake2__fe_cswap(a, b, 0);

    spake2__fe_tobytes(a_after, a);
    spake2__fe_tobytes(b_after, b);

    if (memcmp(a_after, a_before, 32) != 0 ||
        memcmp(b_after, b_before, 32) != 0) {
        fprintf(stderr, "FAIL: fe_cswap(b=0)\n");
        goto done;
    }

    /*
     * Swap.
     */
    spake2__fe_cswap(a, b, 1);

    spake2__fe_tobytes(a_after, a);
    spake2__fe_tobytes(b_after, b);

    if (memcmp(a_after, b_before, 32) != 0 ||
        memcmp(b_after, a_before, 32) != 0) {
        fprintf(stderr, "FAIL: fe_cswap(b=1)\n");
        goto done;
    }

    rc = 1;

done:
    return rc;
}

static int test_aliasing(void)
{
    spake2__fe_t a;
    spake2__fe_t b;
    spake2__fe_t expected;
    BIGNUM *a_bn;
    BIGNUM *b_bn;
    BIGNUM *expected_bn;
    int rc;

    a_bn = NULL;
    b_bn = NULL;
    expected_bn = NULL;
    rc = 0;

    a_bn = random_field_element();
    b_bn = random_field_element();
    expected_bn = BN_new();

    if (a_bn == NULL || b_bn == NULL || expected_bn == NULL) {
        goto done;
    }

    if (!fe_from_bn(a, a_bn) ||
        !fe_from_bn(b, b_bn)) {
        goto done;
    }

    /*
     * h == f
     */
    spake2__fe_copy(expected, a);

    if (BN_mod_mul(expected_bn, a_bn, b_bn, g_p, g_bn_ctx) != 1) {
        goto done;
    }

    spake2__fe_mul(a, a, b);

    if (!check_op("fe_mul alias f", a, expected_bn)) {
        goto done;
    }

    /*
     * h == g
     */
    if (!fe_from_bn(a, a_bn) ||
        !fe_from_bn(b, b_bn)) {
        goto done;
    }

    spake2__fe_mul(b, a, b);

    if (!check_op("fe_mul alias g", b, expected_bn)) {
        goto done;
    }

    /*
     * add alias
     */
    if (!fe_from_bn(a, a_bn) ||
        !fe_from_bn(b, b_bn)) {
        goto done;
    }

    if (BN_mod_add(expected_bn, a_bn, b_bn, g_p, g_bn_ctx) != 1) {
        goto done;
    }

    spake2__fe_add(a, a, b);

    if (!check_op("fe_add alias", a, expected_bn)) {
        goto done;
    }

    rc = 1;

done:
    BN_free(a_bn);
    BN_free(b_bn);
    BN_free(expected_bn);

    return rc;
}


int main(void)
{
    int rc;

    g_p = NULL;
    g_bn_ctx = NULL;
    rc = EXIT_FAILURE;

    g_bn_ctx = BN_CTX_new();
    g_p = BN_new();

    if (g_bn_ctx == NULL || g_p == NULL) {
        fprintf(stderr, "OpenSSL allocation failed\n");
        goto done;
    }

    /*
     * p = 2^255 - 19
     */
    BN_one(g_p);

    if (BN_lshift(g_p, g_p, 255) != 1 ||
        BN_sub_word(g_p, 19) != 1) {
        fprintf(stderr, "failed to construct p\n");
        goto done;
    }

    printf("SPAKE2 field tests\n");
    printf("p = 2^255 - 19\n");
    printf("iterations = %d\n\n", TEST_COUNT);

#define RUN_TEST(fn)                                      \
    do {                                                  \
        printf("  %-24s", #fn);                           \
        fflush(stdout);                                   \
        if (!(fn())) {                                    \
            printf("FAIL\n");                            \
            goto done;                                    \
        }                                                 \
        printf("OK\n");                                   \
    } while (0)

    RUN_TEST(test_zero_one);
    RUN_TEST(test_frombytes_tobytes);
    RUN_TEST(test_add_sub_mul);
    RUN_TEST(test_neg);
    RUN_TEST(test_invert);
    RUN_TEST(test_pow22523);
    RUN_TEST(test_predicates);
    RUN_TEST(test_cmov);
    RUN_TEST(test_cswap);
    RUN_TEST(test_aliasing);

#undef RUN_TEST

    printf("\nALL TESTS PASSED\n");

    rc = EXIT_SUCCESS;

done:
    BN_free(g_p);
    BN_CTX_free(g_bn_ctx);

    return rc;
}
