#include "spake2.h"

#include <stdlib.h>
#include <string.h>

#include "spake2_sha512.h"
#include "spake2_sc.h"
#include "spake2_ge.h"

#define SPAKE2_SCALAR_LEN        32
#define SPAKE2_MSG_LEN           32

typedef enum
{
    SPAKE2_STATE_INIT,
    SPAKE2_STATE_MSG_GENERATED,
    SPAKE2_STATE_KEY_GENERATED,
} spake2_state_t;

typedef struct spake2_ctx
{
    spake2_allocator_t allocator;
    spake2_role_t my_role;
    spake2_state_t state;

    uint8_t *my_name;
    size_t my_name_len;
    uint8_t *their_name;
    size_t their_name_len;

    uint8_t private_key[SPAKE2_SCALAR_LEN];

    uint8_t password_hash[SPAKE2__SHA512_DIGEST_LENGTH];
    spake2__sc_t password_scalar;

    uint8_t my_msg[SPAKE2_MSG_LEN];

    int disable_password_scalar_hack;
} spake2_ctx_t;

typedef void *(*spake2__memset_callback_t)(void *, int, size_t);
static volatile spake2__memset_callback_t spake2__memset_callback = memset;

static void spake2__cleanese(
        void *p, 
        const size_t len) {
    spake2__memset_callback(p, 0, len);
}


static void *spake2__memdup(
        const spake2_allocator_t *allocator,
        const void *p,
        const size_t size)
{
    void *new_p = allocator->malloc(
            allocator->userdata, 
            size);
    if(!new_p)
        return NULL;

    memcpy(new_p, p, size);
    return new_p;
}

static void *spake2__default_malloc(
        void *userdata,
        size_t size)
{
    (void)userdata;
    return malloc(size);
}

static void spake2__default_free(
        void *userdata,
        void *p)
{
    (void)userdata;
    free(p);
}

spake2_ctx_t *spake2_ctx_new(
        const spake2_allocator_t *allocator,
        const spake2_role_t my_role,
        const uint8_t *my_name,
        const size_t my_name_len,
        const uint8_t *their_name,
        const size_t their_name_len)
{
    spake2_allocator_t alloca = {0};
    spake2_ctx_t *ctx = NULL;

    if(
            (my_role < 0 || my_role >= SPAKE2__ROLE_COUNT) ||
            (!my_name ^ (my_name_len > 0)) ||
            (!their_name ^ (their_name_len > 0)))
        return NULL;
    
    if(!allocator)
    {
        alloca.malloc = spake2__default_malloc;
        alloca.free = spake2__default_free;
    } else
        alloca = *allocator;

    ctx = alloca.malloc(
            alloca.userdata, 
            sizeof(*ctx));
    if(!ctx)
        return NULL;
    memset(ctx, 0, sizeof(*ctx));

    ctx->my_name = spake2__memdup(&alloca, 
            my_name, my_name_len);
    ctx->their_name = spake2__memdup(&alloca, 
            their_name, their_name_len);
    if(!ctx->my_name || !ctx->their_name)
    {
        allocator->free(allocator->userdata, ctx->my_name);
        allocator->free(allocator->userdata, ctx->their_name);
        allocator->free(allocator->userdata,ctx);
        return NULL;
    }

    ctx->allocator = alloca;
    ctx->my_role = my_role;
    ctx->my_name_len = my_name_len;
    ctx->their_name_len = their_name_len;

    return ctx;
}

static void spake2__sc_lfshift3(
        spake2__sc_t n) 
{
    uint8_t carry = 0;
    for(int i = 0; i < SPAKE2__SC_LIMB_COUNT; i++) 
    {
        const uint8_t next_carry = n[i] >> 5;
        n[i] = (n[i] << 3) | carry;
        carry = next_carry;
    }
}

_Static_assert(SPAKE2_RANDOM_DATA_LENGTH == sizeof(spake2__sc_wide_t), 
        "random data length doesn't match wide-limb storage");
_Static_assert(SPAKE2__SHA512_DIGEST_LENGTH == sizeof(spake2__sc_wide_t),
        "sha512 digest length doesn't match wide-limb storage");
static const spake2__sc_t spake2__order = 
{
    0xed, 0xd3, 0xf5, 0x5c,
    0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2,
    0xde, 0xf9, 0x4d, 0x14,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10,
};



inline uint64_t constant_time_msb_w(uint64_t a) {
  return 0u - (a >> (sizeof(a) * 8 - 1));
}

inline uint64_t constant_time_lt_w(uint64_t a, uint64_t b) {
    return constant_time_msb_w(a ^ ((a ^ b) | ((a - b) ^ a)));
}

inline uint8_t constant_time_lt_8(uint64_t a, uint64_t b) {
  return (uint8_t)(constant_time_lt_w(a, b));
}

inline uint64_t constant_time_ge_w(uint64_t a, uint64_t b) {
  return ~constant_time_lt_w(a, b);
}

inline uint8_t constant_time_ge_8(uint64_t a, uint64_t b) {
  return (uint8_t)(constant_time_ge_w(a, b));
}

inline uint64_t constant_time_is_zero_w(uint64_t a) {
                      return constant_time_msb_w(~a & (a - 1));
}

inline uint8_t constant_time_is_zero_8(uint64_t a) {
  return (uint8_t)(constant_time_is_zero_w(a));
}

inline uint64_t constant_time_eq_w(uint64_t a, uint64_t b) {
  return constant_time_is_zero_w(a ^ b);
}

int spake2_generate_msg(
        spake2_ctx_t *ctx, 
        uint8_t *out, 
        size_t *out_len,
        const size_t max_out_len, 
        const uint8_t *password,
        const size_t password_len,
        const uint8_t random_data[SPAKE2_RANDOM_DATA_LENGTH])
{
    uint8_t private_tmp[SPAKE2_RANDOM_DATA_LENGTH] = {0};
    uint8_t password_tmp[SPAKE2__SHA512_DIGEST_LENGTH] = {0};
    spake2__sc_t password_scalar = {0};
    spake2__ge_p3 P = {0};
    if(
            !ctx || !out || !out_len || 
            max_out_len < sizeof(ctx->my_msg) || 
            (!password ^ (password_len > 0)) ||
            !random_data)
        return 0;
    
    memcpy(private_tmp, random_data, SPAKE2_RANDOM_DATA_LENGTH);
    spake2__sc_reduce(private_tmp);
    spake2__sc_lfshift3(private_tmp);
    spake2__sc_copy(ctx->private_key, private_tmp);

    spake2__ge_scalarmult_base(&P, ctx->private_key);
    spake2__sha512(password, password_len, password_tmp);
    memcpy(ctx->password_hash, password_tmp, SPAKE2__SHA512_DIGEST_LENGTH);
    spake2__sc_reduce(password_tmp);
    spake2__sc_copy(password_scalar, password_tmp);

    if(!ctx->disable_password_scalar_hack)
    {
    }
    return 1;
}

void spake2_ctx_free(
        spake2_ctx_t *ctx)
{
    spake2_allocator_t allocator = {0};
    if(!ctx)
        return;
    
    allocator = ctx->allocator;
    allocator.free(allocator.userdata, ctx->my_name);
    allocator.free(allocator.userdata, ctx->their_name);
    spake2__cleanese(ctx, sizeof(*ctx));
    allocator.free(allocator.userdata, ctx);
}
