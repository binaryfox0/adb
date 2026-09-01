#ifndef SPAKE2_SHA512_H
#define SPAKE2_SHA512_H

#include <stdint.h>
#include <stddef.h>

#define SPAKE2__SHA512_DIGEST_LENGTH 64

typedef struct
{
    uint64_t h[8];
    uint64_t bitlen[2];
    uint8_t  block[128];
    size_t   block_len;
} spake2__sha512_ctx_t;

void spake2__sha512_init(
        spake2__sha512_ctx_t *ctx);

void spake2__sha512_update(
        spake2__sha512_ctx_t *ctx,
        const void *data,
        const size_t len);

void spake2__sha512_finish(
        spake2__sha512_ctx_t *ctx,
        uint8_t out[SPAKE2__SHA512_DIGEST_LENGTH]);

void spake2__sha512(
        const void *data, 
        const size_t len,
        uint8_t out[SPAKE2__SHA512_DIGEST_LENGTH]);

#endif
