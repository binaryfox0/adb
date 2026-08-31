#ifndef SPAKE2_SHA512_H
#define SPAKE2_SHA512_H

#include <stdint.h>
#include <stddef.h>

#define SPAKE2__SHA512_DIGEST_LENGTH 64

void spake2__sha512(
        const void *data, 
        const size_t len,
        uint8_t out[SPAKE2__SHA512_DIGEST_LENGTH]);

#endif
