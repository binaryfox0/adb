#ifndef SPAKE2_H
#define SPAKE2_H

#include <stdint.h>
#include <stddef.h>

#define SPAKE2_SHA512_DIGEST_LEN 64
#define SPAKE2_SCALAR_LEN        32
#define SPAKE2_MSG_LEN           32

typedef enum
{
    SPAKE2_ROLE_ALICE,
    SPAKE2_ROLE_BOB,
} spake2_role_t;

typedef enum
{
    SPAKE2_STATE_INIT,
    SPAKE2_STATE_MSG_GENERATED,
    SPAKE2_STATE_KEY_GENERATED,
} spake2_state_t;

typedef struct
{
    spake2_role_t my_role;
    spake2_state_t state;

    uint8_t *my_name;
    size_t my_name_len;

    uint8_t *their_name;
    size_t their_name_len;

    uint8_t priv_key[SPAKE2_SCALAR_LEN];

    uint8_t passwd_hash[SPAKE2_SHA512_DIGEST_LEN];
    uint8_t passwd_scalar[SPAKE2_SCALAR_LEN];

    uint8_t my_msg[SPAKE2_MSG_LEN];

    int disable_password_scalar_hack;
} spake2_ctx_t;



#endif
