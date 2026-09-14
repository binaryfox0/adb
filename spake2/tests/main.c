#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <aparse.h>

#include "bssl_spake2.h"
#include "my_spake2.h"

#define info aparse_prog_info
#define error aparse_prog_error

#define TEST_PASSWORD_SIZE 70
#define MAX_BYTES 64

static void print_hex(
        const char *name,
        const uint8_t *data,
        size_t len)
{
    char buffer[MAX_BYTES * 2 + 1] = {0};
    for(size_t i = 0; i < (len > MAX_BYTES ? MAX_BYTES : len); i++) 
    {
        snprintf(buffer + i * 2, sizeof(buffer) - i * 2,
                "%02x", data[i]);
    }
    info("%s: %s", name, buffer);
}

#define TEST_NAME "BSSL Alice <-> MY Bob"
static int test_bssl_alice_my_bob(
        const uint8_t *password,
        size_t password_len)
{
    static const uint8_t my_name[] = "adb pair client";
    static const uint8_t their_name[] = "adb pair server";

    bssl_spake2_t *bssl = NULL;
    my_spake2_t *my = NULL;
    
    uint8_t my_msg[32] = {0};
    size_t my_msg_len = 0;
    uint8_t my_key[64] = {0};
    size_t my_key_len = 0;

    uint8_t bssl_msg[32] = {0};
    size_t bssl_msg_len = 0;
    uint8_t bssl_key[64] = {0};
    size_t bssl_key_len = 0;

    int result = 0;

    bssl = bssl_spake2_new(
        1,
        my_name, sizeof(my_name),
        their_name, sizeof(their_name));

    my = my_spake2_new(
        0,
        their_name, sizeof(their_name),
        my_name, sizeof(my_name));

    if (bssl == NULL || my == NULL)
        goto cleanup;

    if (!bssl_spake2_generate_msg(
            bssl,
            1,
            bssl_msg,
            &bssl_msg_len,
            sizeof(bssl_msg),
            password,
            password_len))
        goto cleanup;

    if (!my_spake2_generate_msg(
            my,
            0,
            my_msg,
            &my_msg_len,
            sizeof(my_msg),
            password,
            password_len))
        goto cleanup;

    if (!bssl_spake2_process_msg(
            bssl,
            bssl_key,
            &bssl_key_len,
            sizeof(bssl_key),
            my_msg,
            my_msg_len))
        goto cleanup;

    if (!my_spake2_process_msg(
            my,
            my_key,
            &my_key_len,
            sizeof(my_key),
            bssl_msg,
            bssl_msg_len))
        goto cleanup;

    if (bssl_key_len != my_key_len) 
    {
        error("%s: FAIL: key lengths differ: BSSL=%zu MY=%zu",
              TEST_NAME, bssl_key_len, my_key_len);
        goto cleanup;
    }

    if (memcmp(bssl_key, my_key, bssl_key_len) != 0) 
    {
        error("%s: FAIL: key mismatch", TEST_NAME);
        print_hex("BSSL key", bssl_key, bssl_key_len);
        print_hex("MY key", my_key, my_key_len);
        goto cleanup;
    }

    result = 1;
    info("%s: PASSED", TEST_NAME);

cleanup:
    printf("\n");
    bssl_spake2_free(bssl);
    my_spake2_free(my);
    return result;
}

#undef TEST_NAME

#define TEST_NAME "MY Alice <-> BSSL Bob"
static int test_my_alice_bssl_bob(
        const uint8_t *password,
        size_t password_len)
{
    static const uint8_t my_name[] = "adb pair client";
    static const uint8_t their_name[] = "adb pair server";

    bssl_spake2_t *bssl = NULL;
    my_spake2_t *my = NULL;
    
    uint8_t my_msg[32] = {0};
    size_t my_msg_len = 0;
    uint8_t my_key[64] = {0};
    size_t my_key_len = 0;
    
    uint8_t bssl_msg[32] = {0};
    size_t bssl_msg_len = 0;
    uint8_t bssl_key[64] = {0};
    size_t bssl_key_len = 0;

    int result = 0;

    my = my_spake2_new(
        0,
        their_name, sizeof(their_name),
        my_name, sizeof(my_name));
    
    bssl = bssl_spake2_new(
        1,
        my_name, sizeof(my_name),
        their_name, sizeof(their_name));

    if (bssl == NULL || my == NULL)
        goto cleanup;

    if(!my_spake2_generate_msg(
                my,
                1,
                my_msg,
                &my_msg_len,
                sizeof(my_msg),
                password,
                password_len))
        goto cleanup;

    if(!bssl_spake2_generate_msg(
                bssl,
                0,
                bssl_msg,
                &bssl_msg_len,
                sizeof(bssl_msg),
                password,
                password_len))
        goto cleanup;

    if(!my_spake2_process_msg(
                my,
                my_key,
                &my_key_len,
                sizeof(my_key),
                bssl_msg,
                bssl_msg_len))
        goto cleanup;

    if(!bssl_spake2_process_msg(
                bssl,
                bssl_key,
                &bssl_key_len,
                sizeof(bssl_key),
                my_msg,
                my_msg_len))
        goto cleanup;

    if (bssl_key_len != my_key_len) 
    {
        error("%s: FAIL: key lengths differ: MY=%zu BSSL=%zu",
              TEST_NAME, my_key_len, bssl_key_len);
        goto cleanup;
    }

    if (memcmp(my_key, bssl_key, my_key_len) != 0) 
    {
        error("%s: FAIL: key mismatch", TEST_NAME);
        print_hex("MY key", my_key, my_key_len);
        print_hex("BSSL key", bssl_key, bssl_key_len);
        goto cleanup;
    }

    result = 1;
    info("%s: PASSED", TEST_NAME);

cleanup:
    printf("\n");
    bssl_spake2_free(bssl);
    my_spake2_free(my);
    return result;
}

static void bssl_reference(
        const uint8_t *password,
        size_t password_len)
{
    static const uint8_t my_name[] = "adb pair client";
    static const uint8_t their_name[] = "adb pair server";

    bssl_spake2_t *alice = NULL;
    bssl_spake2_t *bob = NULL;

    uint8_t bob_msg[32] = {0};
    size_t bob_msg_len = 0;
    uint8_t bob_key[64] = {0};
    size_t bob_key_len = 0;

    uint8_t alice_msg[32] = {0};
    size_t alice_msg_len = 0;
    uint8_t alice_key[64] = {0};
    size_t alice_key_len = 0;
   
    alice = bssl_spake2_new(
            1, 
            my_name, sizeof(my_name),
            their_name, sizeof(their_name));
    bob = bssl_spake2_new(
            0, 
            their_name, sizeof(their_name),
            my_name, sizeof(my_name));
    if (alice == NULL || bob == NULL)
        goto cleanup;

    if(!bssl_spake2_generate_msg(
                alice,
                1,
                alice_msg,
                &alice_msg_len,
                sizeof(alice_msg),
                password,
                password_len))
        goto cleanup;
    
    if(!bssl_spake2_generate_msg(
                bob,
                0,
                bob_msg,
                &bob_msg_len,
                sizeof(bob_msg),
                password,
                password_len))
        goto cleanup;

    if(!bssl_spake2_process_msg(
                alice,
                alice_key,
                &alice_key_len,
                sizeof(alice_key),
                bob_msg,
                bob_msg_len))
        goto cleanup;
    
    if(!bssl_spake2_process_msg(
                bob,
                bob_key,
                &bob_key_len,
                sizeof(bob_key),
                alice_msg,
                alice_msg_len))
        goto cleanup;

    print_hex("Alice key", alice_key, alice_key_len);
    print_hex("Bob key", bob_key, bob_key_len);

cleanup:
    printf("\n");
    bssl_spake2_free(alice);
    bssl_spake2_free(bob);
}

int main(void)
{
    uint8_t password[TEST_PASSWORD_SIZE];
    size_t i;
    int result;

    password[0] = '5';
    password[1] = '1';
    password[2] = '5';
    password[3] = '1';
    password[4] = '0';
    password[5] = '9';

    for (i = 6; i < sizeof(password); i++) {
        password[i] = (uint8_t)i;
    }

    result = 1;

    printf("SPAKE2 BoringSSL interoperability test\n");
    printf("password length: %zu\n\n", sizeof(password));

    test_bssl_alice_my_bob(
            password,
            sizeof(password));

    test_my_alice_bssl_bob(
            password,
            sizeof(password));
    bssl_reference(password, sizeof(password));

    return 1;
}
