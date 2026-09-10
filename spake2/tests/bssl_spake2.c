#include "bssl_spake2.h"

#include <openssl/curve25519.h>

int bssl_spake2(
        int alice,
        const uint8_t *password,
        size_t password_len,
        uint8_t *msg,
        size_t *msg_len,
        uint8_t *key,
        size_t *key_len,
        const uint8_t *peer_msg,
        size_t peer_msg_len)
{
    SPAKE2_CTX *ctx = NULL;
    const uint8_t *my_name = NULL;
    const uint8_t *their_name = NULL;
    size_t my_name_len = 0;
    size_t their_name_len = 0;
    int role = 0;
    int result = 0;

    static const uint8_t client_name[] = "adb pair client";
    static const uint8_t server_name[] = "adb pair server";

    if (alice != 0) {
        role = spake2_role_alice;
        my_name = client_name;
        my_name_len = sizeof(client_name);

        their_name = server_name;
        their_name_len = sizeof(server_name);
    } else {
        role = spake2_role_bob;
        my_name = server_name;
        my_name_len = sizeof(server_name);

        their_name = client_name;
        their_name_len = sizeof(client_name);
    }

    ctx = SPAKE2_CTX_new(
            role,
            my_name,
            my_name_len,
            their_name,
            their_name_len);

    if (!ctx)
        return 0;

    if (!SPAKE2_generate_msg(
                ctx,
                msg,
                msg_len,
                SPAKE2_MAX_MSG_SIZE,
                password,
                password_len)) {
        SPAKE2_CTX_free(ctx);
        return 0;
    }

    if (peer_msg) {
        if (!SPAKE2_process_msg(
                    ctx,
                    key,
                    key_len,
                    SPAKE2_MAX_KEY_SIZE,
                    peer_msg,
                    peer_msg_len)) {
            SPAKE2_CTX_free(ctx);
            return 0;
        }
    }

    result = 1;

    SPAKE2_CTX_free(ctx);

    return result;
}
