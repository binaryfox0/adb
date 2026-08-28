#include "spake2_sha512.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef struct
{
    uint64_t h[8];
    uint64_t bitlen[2];
    uint8_t  block[128];
    size_t   block_len;
} spake2__sha512_ctx_t;

static const uint64_t spake2__sha512_k[80] =
{
    UINT64_C(0x428a2f98d728ae22), UINT64_C(0x7137449123ef65cd),
    UINT64_C(0xb5c0fbcfec4d3b2f), UINT64_C(0xe9b5dba58189dbbc),
    UINT64_C(0x3956c25bf348b538), UINT64_C(0x59f111f1b605d019),
    UINT64_C(0x923f82a4af194f9b), UINT64_C(0xab1c5ed5da6d8118),
    UINT64_C(0xd807aa98a3030242), UINT64_C(0x12835b0145706fbe),
    UINT64_C(0x243185be4ee4b28c), UINT64_C(0x550c7dc3d5ffb4e2),
    UINT64_C(0x72be5d74f27b896f), UINT64_C(0x80deb1fe3b1696b1),
    UINT64_C(0x9bdc06a725c71235), UINT64_C(0xc19bf174cf692694),
    UINT64_C(0xe49b69c19ef14ad2), UINT64_C(0xefbe4786384f25e3),
    UINT64_C(0x0fc19dc68b8cd5b5), UINT64_C(0x240ca1cc77ac9c65),
    UINT64_C(0x2de92c6f592b0275), UINT64_C(0x4a7484aa6ea6e483),
    UINT64_C(0x5cb0a9dcbd41fbd4), UINT64_C(0x76f988da831153b5),
    UINT64_C(0x983e5152ee66dfab), UINT64_C(0xa831c66d2db43210),
    UINT64_C(0xb00327c898fb213f), UINT64_C(0xbf597fc7beef0ee4),
    UINT64_C(0xc6e00bf33da88fc2), UINT64_C(0xd5a79147930aa725),
    UINT64_C(0x06ca6351e003826f), UINT64_C(0x142929670a0e6e70),
    UINT64_C(0x27b70a8546d22ffc), UINT64_C(0x2e1b21385c26c926),
    UINT64_C(0x4d2c6dfc5ac42aed), UINT64_C(0x53380d139d95b3df),
    UINT64_C(0x650a73548baf63de), UINT64_C(0x766a0abb3c77b2a8),
    UINT64_C(0x81c2c92e47edaee6), UINT64_C(0x92722c851482353b),
    UINT64_C(0xa2bfe8a14cf10364), UINT64_C(0xa81a664bbc423001),
    UINT64_C(0xc24b8b70d0f89791), UINT64_C(0xc76c51a30654be30),
    UINT64_C(0xd192e819d6ef5218), UINT64_C(0xd69906245565a910),
    UINT64_C(0xf40e35855771202a), UINT64_C(0x106aa07032bbd1b8),
    UINT64_C(0x19a4c116b8d2d0c8), UINT64_C(0x1e376c085141ab53),
    UINT64_C(0x2748774cdf8eeb99), UINT64_C(0x34b0bcb5e19b48a8),
    UINT64_C(0x391c0cb3c5c95a63), UINT64_C(0x4ed8aa4ae3418acb),
    UINT64_C(0x5b9cca4f7763e373), UINT64_C(0x682e6ff3d6b2b8a3),
    UINT64_C(0x748f82ee5defb2fc), UINT64_C(0x78a5636f43172f60),
    UINT64_C(0x84c87814a1f0ab72), UINT64_C(0x8cc702081a6439ec),
    UINT64_C(0x90befffa23631e28), UINT64_C(0xa4506cebde82bde9),
    UINT64_C(0xbef9a3f7b2c67915), UINT64_C(0xc67178f2e372532b),
    UINT64_C(0xca273eceea26619c), UINT64_C(0xd186b8c721c0c207),
    UINT64_C(0xeada7dd6cde0eb1e), UINT64_C(0xf57d4f7fee6ed178),
    UINT64_C(0x06f067aa72176fba), UINT64_C(0x0a637dc5a2c898a6),
    UINT64_C(0x113f9804bef90dae), UINT64_C(0x1b710b35131c471b),
    UINT64_C(0x28db77f523047d84), UINT64_C(0x32caab7b40c72493),
    UINT64_C(0x3c9ebe0a15c9bebc), UINT64_C(0x431d67c49c100d4c),
    UINT64_C(0x4cc5d4becb3e42b6), UINT64_C(0x597f299cfc657e2a),
    UINT64_C(0x5fcb6fab3ad6faec), UINT64_C(0x6c44198c4a475817),
};

static uint64_t spake2__sha512_rotr(
        const uint64_t x,
        const uint32_t n)
{
    return (x >> n) | (x << (64u - n));
}

static uint64_t spake2__sha512_ch(
        const uint64_t x,
        const uint64_t y,
        const uint64_t z) {
    return (x & y) ^ (~x & z);
}

static uint64_t spake2__sha512_maj(
        const uint64_t x,
        const uint64_t y,
        const uint64_t z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

static uint64_t spake2__sha512_big_sigma0(
        const uint64_t x)
{
    return spake2__sha512_rotr(x, 28)
         ^ spake2__sha512_rotr(x, 34)
         ^ spake2__sha512_rotr(x, 39);
}

static uint64_t spake2__sha512_big_sigma1(
        const uint64_t x)
{
    return spake2__sha512_rotr(x, 14)
         ^ spake2__sha512_rotr(x, 18)
         ^ spake2__sha512_rotr(x, 41);
}

static uint64_t spake2__sha512_small_sigma0(
        const uint64_t x)
{
    return spake2__sha512_rotr(x, 1)
         ^ spake2__sha512_rotr(x, 8)
         ^ (x >> 7);
}

static uint64_t spake2__sha512_small_sigma1(
        const uint64_t x)
{
    return spake2__sha512_rotr(x, 19)
         ^ spake2__sha512_rotr(x, 61)
         ^ (x >> 6);
}

static uint64_t spake2__sha512_load64(
        const uint8_t *p)
{
    return ((uint64_t)p[0] << 56)
         | ((uint64_t)p[1] << 48)
         | ((uint64_t)p[2] << 40)
         | ((uint64_t)p[3] << 32)
         | ((uint64_t)p[4] << 24)
         | ((uint64_t)p[5] << 16)
         | ((uint64_t)p[6] << 8)
         | ((uint64_t)p[7]);
}

static void spake2__sha512_store64(
        uint8_t *p,
        uint64_t x)
{
    p[0] = (uint8_t)(x >> 56);
    p[1] = (uint8_t)(x >> 48);
    p[2] = (uint8_t)(x >> 40);
    p[3] = (uint8_t)(x >> 32);
    p[4] = (uint8_t)(x >> 24);
    p[5] = (uint8_t)(x >> 16);
    p[6] = (uint8_t)(x >> 8);
    p[7] = (uint8_t)x;
}

static void spake2__sha512_transform(
        spake2__sha512_ctx_t *ctx,
        const uint8_t block[128])
{
    uint64_t w[80] = {0};
    uint64_t 
        a = 0, b = 0, c = 0, 
        d = 0, e = 0, f = 0, 
        g = 0, h = 0;
    uint64_t t1 = 0, t2 = 0;

    for(int i = 0; i < 16; i++)
        w[i] = spake2__sha512_load64(block + i * 8);

    for(int i = 16; i < 80; i++)
    {
        w[i] = spake2__sha512_small_sigma1(w[i - 2])
             + w[i - 7]
             + spake2__sha512_small_sigma0(w[i - 15])
             + w[i - 16];
    }

    a = ctx->h[0];
    b = ctx->h[1];
    c = ctx->h[2];
    d = ctx->h[3];
    e = ctx->h[4];
    f = ctx->h[5];
    g = ctx->h[6];
    h = ctx->h[7];

    for(int i = 0; i < 80; i++)
    {
        t1 = h
            + spake2__sha512_big_sigma1(e)
            + spake2__sha512_ch(e, f, g)
            + spake2__sha512_k[i]
            + w[i];

        t2 = spake2__sha512_big_sigma0(a)
            + spake2__sha512_maj(a, b, c);

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->h[0] += a;
    ctx->h[1] += b;
    ctx->h[2] += c;
    ctx->h[3] += d;
    ctx->h[4] += e;
    ctx->h[5] += f;
    ctx->h[6] += g;
    ctx->h[7] += h;
}

static void spake2__sha512_init(
        spake2__sha512_ctx_t *ctx)
{
    ctx->h[0] = UINT64_C(0x6a09e667f3bcc908);
    ctx->h[1] = UINT64_C(0xbb67ae8584caa73b);
    ctx->h[2] = UINT64_C(0x3c6ef372fe94f82b);
    ctx->h[3] = UINT64_C(0xa54ff53a5f1d36f1);
    ctx->h[4] = UINT64_C(0x510e527fade682d1);
    ctx->h[5] = UINT64_C(0x9b05688c2b3e6c1f);
    ctx->h[6] = UINT64_C(0x1f83d9abfb41bd6b);
    ctx->h[7] = UINT64_C(0x5be0cd19137e2179);

    ctx->bitlen[0] = 0;
    ctx->bitlen[1] = 0;
    ctx->block_len = 0;
}

static void spake2__sha512_add_bits(
        spake2__sha512_ctx_t *ctx,
        uint64_t bits)
{
    uint64_t old = ctx->bitlen[1];
    ctx->bitlen[1] += bits;
    if (ctx->bitlen[1] < old)
        ctx->bitlen[0]++;
}

static void spake2__sha512_update(
        spake2__sha512_ctx_t *ctx,
        const void *data,
        size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    while(len != 0)
    {
        size_t n = sizeof(ctx->block) - ctx->block_len;
        if (n > len) n = len;

        memcpy(ctx->block + ctx->block_len, p, n);
        ctx->block_len += n;
        p += n, len -= n;

        if (ctx->block_len == sizeof(ctx->block))
        {
            spake2__sha512_transform(ctx, ctx->block);
            spake2__sha512_add_bits(ctx, UINT64_C(1024));
            ctx->block_len = 0;
        }
    }
}

static void spake2__sha512_finish(
        spake2__sha512_ctx_t *ctx,
        uint8_t out[SPAKE2__SHA512_DIGEST_LEN])
{
    size_t n = 0;
    spake2__sha512_add_bits(
        ctx, (uint64_t)ctx->block_len * UINT64_C(8));

    n = ctx->block_len;
    ctx->block[n++] = 0x80;
    if (n > 112)
    {
        while(n < 128) ctx->block[n++] = 0;
        spake2__sha512_transform(ctx, ctx->block);
        n = 0;
    }

    while (n < 112) ctx->block[n++] = 0;
    spake2__sha512_store64(
        ctx->block + 112, ctx->bitlen[0]);
    spake2__sha512_store64(
        ctx->block + 120, ctx->bitlen[1]);
    spake2__sha512_transform(ctx, ctx->block);

    for(int i = 0; i < 8; i++)
        spake2__sha512_store64(out + i * 8, ctx->h[i]);

    memset(ctx, 0, sizeof(*ctx));
}

void spake2__sha512(
        const void *data,
        const size_t len,
        uint8_t out[SPAKE2__SHA512_DIGEST_LEN])
{
    spake2__sha512_ctx_t ctx = {0};

    spake2__sha512_init(&ctx);
    spake2__sha512_update(&ctx, data, len);
    spake2__sha512_finish(&ctx, out);
}
