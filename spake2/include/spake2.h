#ifndef SPAKE2_H
#define SPAKE2_H

#include <stdint.h>
#include <stddef.h>

#define SPAKE2_RANDOM_DATA_LENGTH 64

typedef enum
{
    SPAKE2_ROLE_ALICE,
    SPAKE2_ROLE_BOB,
    SPAKE2__ROLE_COUNT
} spake2_role_t;

typedef struct
{
    void *(*malloc)(void *userdata, size_t size);
    void (*free)(void *userdata, void *p);
    void *userdata;
} spake2_allocator_t;

typedef struct spake2_ctx spake2_ctx_t;

spake2_ctx_t *spake2_ctx_new(
        const spake2_allocator_t *allocator,
        const spake2_role_t my_role,
        const uint8_t *my_name,
        const size_t my_name_len,
        const uint8_t *their_name,
        const size_t their_name_len);

int spake2_generate_msg(
        spake2_ctx_t *ctx, 
        uint8_t *out, 
        size_t *out_len,
        const size_t max_out_len, 
        const uint8_t *password,
        const size_t password_len,
        const uint8_t random_data[SPAKE2_RANDOM_DATA_LENGTH]);

void spake2_ctx_free(
        spake2_ctx_t *ctx);

#endif
