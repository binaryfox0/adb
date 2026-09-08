#ifndef SPAKE2_SC_H
#define SPAKE2_SC_H

#include <stdint.h>

typedef struct {
    uint8_t v[64];
} spake2__sc_wide_t;

typedef struct {
    uint8_t v[32];
} spake2__sc_t;

void spake2__sc_reduce(
        spake2__sc_t *out1,
        const spake2__sc_wide_t *arg1);

void spake2__sc_lshift3(
        spake2__sc_t *out1);

#endif
