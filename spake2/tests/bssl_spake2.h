#ifndef BSSL_SPAKE2_H
#define BSSL_SPAKE2_H

#include <stddef.h>
#include <stdint.h>

typedef void bssl_spake2_t;

bssl_spake2_t *bssl_spake2_new(
        const int is_alice,
        const uint8_t *my_name,
        const size_t my_name_len,
        const uint8_t *their_name,
        const size_t their_name_len);

int bssl_spake2_generate_msg(
        bssl_spake2_t *ctx,
        const int is_alice,
        uint8_t *out, 
        size_t *out_len,
        const size_t max_out_len, 
        const uint8_t *password,
        const size_t password_len);

int bssl_spake2_process_msg(
        bssl_spake2_t *ctx, 
        uint8_t *out_key, 
        size_t *out_key_len,
        const size_t max_out_key_len, 
        const uint8_t *their_msg,
        const size_t their_msg_len);

void bssl_spake2_free(
        bssl_spake2_t *ctx);

#endif
