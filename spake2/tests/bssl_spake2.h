#ifndef BSSL_SPAKE2_H
#define BSSL_SPAKE2_H

#include <stddef.h>
#include <stdint.h>

int bssl_spake2(
        int alice,
        const uint8_t *password,
        size_t password_len,
        uint8_t *msg,
        size_t *msg_len,
        uint8_t *key,
        size_t *key_len,
        const uint8_t *peer_msg,
        size_t peer_msg_len);

#endif
