#include "spake2_fe.h"

#include <openssl/bn.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <aparse.h>

#define info aparse_prog_info
#define error aparse_prog_error

#define TEST_COUNT 100000

static int bn_set_modulus(BIGNUM *p)
{
    int ret = 0;
    if(BN_set_bit(p, 255) != 1)
        goto done;

    if(BN_sub_word(p, 19) != 1)
        goto done;

    ret = 1;

done:
    return ret;
}

static int bn_to_fe(spake2__fe_t r, const BIGNUM *x)
{
    BIGNUM *t = NULL;
    BN_ULONG limb = 0;
    int bits = 0;
    int i = 0;
    int ret = 0;

    t = BN_dup(x);
    if(t == NULL)
        goto done;

    memset(r, 0, sizeof(*r));

    for(i = 0; i < 10; i++) {
        bits = (i & 1) ? 25 : 26;

        limb = BN_mod_word(t, ((BN_ULONG)1 << bits));

        if(limb > INT32_MAX)
            goto done;

        r[i] = (int32_t)limb;

        if(BN_rshift(t, t, bits) != 1)
            goto done;
    }

    /*
     * The input is in [0, p), so all 255 bits must fit
     * exactly into the 10 alternating-radix limbs.
     */
    if(!BN_is_zero(t))
        goto done;

    ret = 1;

done:
    BN_free(t);
    return ret;
}

static int fe_to_bn(BIGNUM *r, const spake2__fe_t a)
{
    BIGNUM *limb = NULL;
    int shift = 0;
    int bits = 0;
    int64_t value = 0;
    int ret = 0;

    limb = BN_new();
    if(limb == NULL)
        goto done;

    BN_zero(r);

    for(int i = 0; i < 10; i++)
    {
        bits = (i & 1) ? 25 : 26;
        value = a[i];

        if(value >= 0) {
            if(BN_set_word(limb, (BN_ULONG)value) != 1)
                goto done;

            if(shift != 0) {
                if(BN_lshift(limb, limb, shift) != 1)
                    goto done;
            }

            if(BN_add(r, r, limb) != 1)
                goto done;
        } else {
            if(BN_set_word(limb, (BN_ULONG)(-value)) != 1)
                goto done;

            if(shift != 0) {
                if(BN_lshift(limb, limb, shift) != 1)
                    goto done;
            }

            if(BN_sub(r, r, limb) != 1)
                goto done;
        }

        shift += bits;
    }

    ret = 1;

done:
    BN_free(limb);
    return ret;
}

static void print_bn(const char *name, const BIGNUM *x)
{
    char *s = BN_bn2hex(x);
    if(!s)
        return;

    info("%s = %s\n", name, s);
    OPENSSL_free(s);
}

static void print_fe_limbs(const spake2__fe_t x)
{
    info(NULL);
    fprintf(stderr, "limbs = [");
    for(int i = 0; i < SPAKE2__FE_LIMB_COUNT; i++) {
        if(i != 0)
            fprintf(stderr, ", ");
        fprintf(stderr, "%" PRId32, x[i]);
    }
    fprintf(stderr, "]\n");
}

static int check_equal(
        BN_CTX *ctx,
        const BIGNUM *p,
        const char *name,
        const spake2__fe_t actual,
        const BIGNUM *expected)
{
    BIGNUM *actual_bn = NULL;
    int ret = 0;

    actual_bn = BN_new();
    if(actual_bn == NULL)
        goto done;

    if(!fe_to_bn(actual_bn, actual))
        goto done;

    /*
     * fe operations use a loose signed-limb representation.
     *
     * BN_nnmod() is important here: BN_mod() may preserve a
     * negative remainder, while field comparison needs [0, p).
     */
    if(BN_nnmod(actual_bn, actual_bn, p, ctx) != 1)
        goto done;

    if(BN_cmp(actual_bn, expected) != 0) 
    {
        error("%s", name);
        print_bn("    expected", expected);
        print_bn("    actual", actual_bn);
        print_fe_limbs(actual);
        goto done;
    }

    ret = 1;

done:
    BN_free(actual_bn);
    return ret;
}

static int test_conversion(
        BN_CTX *ctx,
        const BIGNUM *p,
        const BIGNUM *expected,
        const spake2__fe_t actual)
{
    BIGNUM *actual_bn = NULL;
    int ret = 0;

    actual_bn = BN_new();
    if(actual_bn == NULL)
        goto done;

    if(!fe_to_bn(actual_bn, actual))
        goto done;

    if(BN_nnmod(actual_bn, actual_bn, p, ctx) != 1)
        goto done;

    if(BN_cmp(actual_bn, expected) != 0) 
    {
        error("conversion");
        print_bn("    expected", expected);
        print_bn("    actual", actual_bn);
        print_fe_limbs(actual);
        goto done;
    }

    ret = 1;

done:
    BN_free(actual_bn);

    return ret;
}

static int test_add(
        BN_CTX *ctx,
        const BIGNUM *p,
        const BIGNUM *a_bn,
        const BIGNUM *b_bn,
        const spake2__fe_t a,
        const spake2__fe_t b)
{
    BIGNUM *expected = NULL;
    spake2__fe_t actual;
    int ret = 0;

    expected = BN_new();
    if(expected == NULL)
        goto done;

    if(BN_mod_add(expected, a_bn, b_bn, p, ctx) != 1)
        goto done;

    spake2__fe_add(actual, a, b);
    ret = check_equal(
        ctx,
        p,
        "fe_add",
        actual,
        expected);

done:
    BN_free(expected);

    return ret;
}

static int test_sub(
        BN_CTX *ctx,
        const BIGNUM *p,
        const BIGNUM *a_bn,
        const BIGNUM *b_bn,
        const spake2__fe_t a,
        const spake2__fe_t b)
{
    BIGNUM *expected = NULL;
    spake2__fe_t actual;
    int ret = 0;

    expected = BN_new();
    if(expected == NULL)
        goto done;

    if(BN_mod_sub(expected, a_bn, b_bn, p, ctx) != 1)
        goto done;

    spake2__fe_sub(actual, a, b);

    ret = check_equal(
        ctx,
        p,
        "fe_sub",
        actual,
        expected);

done:
    BN_free(expected);
    return ret;
}

static int test_mul(
        BN_CTX *ctx,
        const BIGNUM *p,
        const BIGNUM *a_bn,
        const BIGNUM *b_bn,
        const spake2__fe_t a,
        const spake2__fe_t b)
{
    BIGNUM *expected = NULL;
    spake2__fe_t actual;
    int ret = 0;

    expected = BN_new();
    if(expected == NULL)
        goto done;

    if(BN_mod_mul(expected, a_bn, b_bn, p, ctx) != 1)
        goto done;

    spake2__fe_mul(actual, a, b);

    ret = check_equal(
        ctx,
        p,
        "fe_mul",
        actual,
        expected);

done:
    BN_free(expected);
    return ret;
}

static int test_mul_alias_a(
        BN_CTX *ctx,
        const BIGNUM *p,
        const BIGNUM *a_bn,
        const BIGNUM *b_bn,
        const spake2__fe_t a,
        const spake2__fe_t b)
{
    BIGNUM *expected = NULL;
    spake2__fe_t actual;
    int ret = 0;

    expected = BN_new();
    if(expected == NULL)
        goto done;

    if(BN_mod_mul(expected, a_bn, b_bn, p, ctx) != 1)
        goto done;

    spake2__fe_copy(actual, a);
    spake2__fe_mul(actual, actual, b);

    ret = check_equal(
        ctx,
        p,
        "fe_mul(r == a)",
        actual,
        expected);

done:
    BN_free(expected);

    return ret;
}

static int test_mul_alias_b(
        BN_CTX *ctx,
        const BIGNUM *p,
        const BIGNUM *a_bn,
        const BIGNUM *b_bn,
        const spake2__fe_t a,
        const spake2__fe_t b)
{
    BIGNUM *expected = NULL;
    spake2__fe_t actual;
    int ret = 0;

    expected = BN_new();
    if(expected == NULL)
        goto done;

    if(BN_mod_mul(expected, a_bn, b_bn, p, ctx) != 1)
        goto done;

    spake2__fe_copy(actual, b);
    spake2__fe_mul(actual, a, actual);

    ret = check_equal(
        ctx,
        p,
        "fe_mul(r == b)",
        actual,
        expected);

done:
    BN_free(expected);

    return ret;
}

static int test_invert(
        BN_CTX *ctx,
        const BIGNUM *p,
        const BIGNUM *a_bn,
        const spake2__fe_t a)
{
    BIGNUM *expected = NULL;
    BIGNUM *actual_bn = NULL;
    spake2__fe_t actual;
    int ret = 0;

    expected = BN_new();
    actual_bn = BN_new();

    if(!expected || !actual_bn)
        goto done;

    /*
     * BN_mod_inverse() returns NULL for zero, so handle zero
     * separately if the FE inversion API defines 0 -> 0.
     */
    if(BN_is_zero(a_bn)) {
        BN_zero(expected);
    } else {
        if(BN_mod_inverse(expected, a_bn, p, ctx) == NULL)
            goto done;
    }

    spake2__fe_invert(actual, a);

    if(!fe_to_bn(actual_bn, actual))
        goto done;

    if(BN_nnmod(actual_bn, actual_bn, p, ctx) != 1)
        goto done;

    if(BN_cmp(actual_bn, expected) != 0) {
        error("fe_invert");
        print_bn("    input", a_bn);
        print_bn("    expected", expected);
        print_bn("    actual", actual_bn);
        print_fe_limbs(actual);
        goto done;
    }

    ret = 1;

done:
    BN_free(actual_bn);
    BN_free(expected);
    return ret;
}

int main(int argc, char **argv)
{
    int failed = 0;
    BN_CTX *ctx = NULL;
    BIGNUM *p = NULL;
    BIGNUM *a_bn = NULL;
    BIGNUM *b_bn = NULL;
    spake2__fe_t a;
    spake2__fe_t b;
    int ret = 1;

    aparse_parse(
            argc, argv, 
            NULL, NULL, 
            "field-element test");

    ctx = BN_CTX_new();
    p = BN_new();
    a_bn = BN_new();
    b_bn = BN_new();

    if(!ctx || !p || !a_bn || !b_bn)
    {
        info("failed to allocate openssl types\n");
        goto done;
    }

    if(!bn_set_modulus(p)) {
        error("failed to construct modulus\n");
        goto done;
    }

    for(int i = 0; i < TEST_COUNT; i++) 
    {
        if(BN_rand_range(a_bn, p) != 1)
        {
            error("failed to generate random BN a");
            goto done;
        }

        if(BN_rand_range(b_bn, p) != 1)
        {
            error("failed to generate random BN b");
            goto done;
        }

        if(!bn_to_fe(a, a_bn))
        {
            error("failed to convert BN a into FE a");
            goto done;
        }

        if(!bn_to_fe(b, b_bn))
        {
            error("failed to convert BN b into FE b");
            goto done;
        }

        /*
         * Verify the test conversion independently before
         * testing arithmetic.
         */
        if(
                !test_conversion(ctx, p, a_bn, a) ||
                !test_conversion(ctx, p, b_bn, b) ||
                !test_add(ctx, p, a_bn, b_bn, a, b) ||
                !test_sub(ctx, p, a_bn, b_bn, a, b) ||
                !test_mul(ctx, p, a_bn, b_bn, a, b) ||
                !test_mul_alias_a(ctx, p, a_bn, b_bn, a, b) ||
                !test_mul_alias_b(ctx, p, a_bn, b_bn, a, b))
            failed++;

        if((i % 10000) == 0) 
            info("tests %d/%d completed", i, TEST_COUNT);        
    }

    info("summary: %d passed, %d failed", 
            TEST_COUNT - failed, failed);

    ret = 0;

done:
    BN_free(b_bn);
    BN_free(a_bn);
    BN_free(p);
    BN_CTX_free(ctx);

    return ret;
}
