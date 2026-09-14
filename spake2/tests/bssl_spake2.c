#include "bssl_spake2.h"

#include <openssl/curve25519.h>

bssl_spake2_t *bssl_spake2_new(
        const int is_alice,
        const uint8_t *my_name,
        const size_t my_name_len,
        const uint8_t *their_name,
        const size_t their_name_len)
{
    return SPAKE2_CTX_new(
            is_alice ? spake2_role_alice : spake2_role_bob, 
            my_name, my_name_len, 
            their_name, their_name_len);
}

int bssl_spake2_generate_msg(
        bssl_spake2_t *ctx,
        const int is_alice,
        uint8_t *out, 
        size_t *out_len,
        const size_t max_out_len, 
        const uint8_t *password,
        const size_t password_len)
{
    (void)is_alice;
    return SPAKE2_generate_msg(
            ctx,
            out, out_len, max_out_len,
            password, password_len);
}

int bssl_spake2_process_msg(
        bssl_spake2_t *ctx, 
        uint8_t *out_key, 
        size_t *out_key_len,
        const size_t max_out_key_len, 
        const uint8_t *their_msg,
        const size_t their_msg_len)
{
    return SPAKE2_process_msg(
            ctx,
            out_key, out_key_len, max_out_key_len,
            their_msg, their_msg_len);
}

void bssl_spake2_free(
        bssl_spake2_t *ctx)
{
    SPAKE2_CTX_free(ctx);
}

