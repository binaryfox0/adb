#include "my_spake2.h"

#include "spake2.h"

#include <string.h>

int my_spake2(
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
uint8_t data[64] = {
    0xA7, 0x3C, 0x91, 0xE2, 0x5B, 0x08, 0xD4, 0x6F,
    0xB1, 0x27, 0xFA, 0x83, 0x4D, 0xC6, 0x19, 0x70,
    0xDE, 0x45, 0xAC, 0x02, 0xF8, 0x31, 0x67, 0x9B,
    0x14, 0xE5, 0x73, 0xCA, 0x58, 0x0F, 0xB9, 0x26,
    0x6A, 0xD3, 0x84, 0x1C, 0xF1, 0x5E, 0xA0, 0x39,
    0xCB, 0x72, 0x04, 0xED, 0x56, 0x98, 0x2B, 0xBF,
    0x63, 0xD8, 0x10, 0x47, 0xF6, 0xA9, 0x35, 0x8C,
    0x21, 0xCE, 0x79, 0x03, 0xB4, 0x5A, 0xE7, 0x16
};
    spake2_ctx_t *ctx;
    spake2_role_t role;
    const uint8_t *my_name;
    const uint8_t *their_name;
    size_t my_name_len;
    size_t their_name_len;
    int result;

    ctx = NULL;
    role = SPAKE2_ROLE_ALICE;
    my_name = NULL;
    their_name = NULL;
    my_name_len = 0;
    their_name_len = 0;
    result = 0;

    static const uint8_t client_name[] = "adb pair client";
    static const uint8_t server_name[] = "adb pair server";

    if (alice != 0) {
        role = SPAKE2_ROLE_ALICE;
        my_name = client_name;
        my_name_len = sizeof(client_name);

        their_name = server_name;
        their_name_len = sizeof(server_name);
    } else {
        role = SPAKE2_ROLE_BOB;
        my_name = server_name;
        my_name_len = sizeof(server_name);

        their_name = client_name;
        their_name_len = sizeof(client_name);
    }

    ctx = spake2_ctx_new(
            NULL,
            role,
            my_name,
            my_name_len,
            their_name,
            their_name_len);

    if (ctx == NULL) {
        return 0;
    }

    if (!spake2_generate_msg(
                ctx,
                msg,
                msg_len,
                SPAKE2_MAX_MESSAGE_LENGTH,
                password,
                password_len,
                data)) {
        spake2_ctx_free(ctx);
        return 0;
    }

    if (peer_msg != NULL) {
        if (!spake2_process_msg(
                    ctx,
                    key,
                    key_len,
                    SPAKE2_MAX_KEY_LENGTH,
                    peer_msg,
                    peer_msg_len)) {
            spake2_ctx_free(ctx);
            return 0;
        }
    }

    result = 1;

    spake2_ctx_free(ctx);

    return result;
}
