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
    uint8_t alice_random[64] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
    };
    
    uint8_t bob_random[64] = {
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
        0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
        0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f
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
                alice ? alice_random : bob_random)) {
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
