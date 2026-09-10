#include "bssl_spake2.h"
#include "my_spake2.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define TEST_PASSWORD_SIZE 70

static void print_hex(
        const char *name,
        const uint8_t *data,
        size_t len)
{
    size_t i;

    printf("%s (%zu bytes):\n", name, len);

    for (i = 0; i < len; i++) {
        printf("%02x", data[i]);

        if ((i + 1) % 32 == 0) {
            putchar('\n');
        }
    }

    if (len % 32 != 0) {
        putchar('\n');
    }
}

static int test_bssl_alice_my_bob(
        const uint8_t *password,
        size_t password_len)
{
    uint8_t bssl_msg[32];
    uint8_t my_msg[32];
    uint8_t bssl_key[64];
    uint8_t my_key[64];
    size_t bssl_msg_len;
    size_t my_msg_len;
    size_t bssl_key_len;
    size_t my_key_len;
    int result;

    bssl_msg_len = 0;
    my_msg_len = 0;
    bssl_key_len = 0;
    my_key_len = 0;
    result = 0;

    /*
     * BoringSSL = Alice
     * My implementation = Bob
     */

    if (!bssl_spake2(
                1,
                password,
                password_len,
                bssl_msg,
                &bssl_msg_len,
                bssl_key,
                &bssl_key_len,
                NULL,
                0)) {
        return 0;
    }

    if (!my_spake2(
                0,
                password,
                password_len,
                my_msg,
                &my_msg_len,
                my_key,
                &my_key_len,
                bssl_msg,
                bssl_msg_len)) {
        return 0;
    }

    /*
     * We still need BoringSSL Alice to process
     * My Bob message. The original BoringSSL
     * context was already consumed, so create
     * a fresh Alice context.
     */

    if (!bssl_spake2(
                1,
                password,
                password_len,
                bssl_msg,
                &bssl_msg_len,
                bssl_key,
                &bssl_key_len,
                my_msg,
                my_msg_len)) {
        return 0;
    }

    if (bssl_key_len != my_key_len) {
        printf("FAIL: key lengths differ: BSSL=%zu MY=%zu\n",
                bssl_key_len,
                my_key_len);
        return 0;
    }

    if (memcmp(bssl_key, my_key, bssl_key_len) != 0) {
        printf("FAIL: key mismatch\n");

        print_hex("BSSL key", bssl_key, bssl_key_len);
        print_hex("MY key", my_key, my_key_len);

        return 0;
    }

    result = 1;

    return result;
}

static int test_my_alice_bssl_bob(
        const uint8_t *password,
        size_t password_len)
{
    uint8_t my_msg[32];
    uint8_t bssl_msg[32];
    uint8_t my_key[64];
    uint8_t bssl_key[64];
    size_t my_msg_len;
    size_t bssl_msg_len;
    size_t my_key_len;
    size_t bssl_key_len;

    my_msg_len = 0;
    bssl_msg_len = 0;
    my_key_len = 0;
    bssl_key_len = 0;

    /*
     * My implementation = Alice
     * BoringSSL = Bob
     */

    if (!my_spake2(
                1,
                password,
                password_len,
                my_msg,
                &my_msg_len,
                my_key,
                &my_key_len,
                NULL,
                0)) {
        return 0;
    }

    if (!bssl_spake2(
                0,
                password,
                password_len,
                bssl_msg,
                &bssl_msg_len,
                bssl_key,
                &bssl_key_len,
                my_msg,
                my_msg_len)) {
        return 0;
    }

    /*
     * Recreate My Alice context and process BSSL Bob's
     * message.
     */

    if (!my_spake2(
                1,
                password,
                password_len,
                my_msg,
                &my_msg_len,
                my_key,
                &my_key_len,
                bssl_msg,
                bssl_msg_len)) {
        return 0;
    }

    if (my_key_len != bssl_key_len) {
        printf("FAIL: key lengths differ: MY=%zu BSSL=%zu\n",
                my_key_len,
                bssl_key_len);
        return 0;
    }

    if (memcmp(my_key, bssl_key, my_key_len) != 0) {
        printf("FAIL: key mismatch\n");

        print_hex("MY key", my_key, my_key_len);
        print_hex("BSSL key", bssl_key, bssl_key_len);

        return 0;
    }

    return 1;
}

int main(void)
{
    uint8_t password[TEST_PASSWORD_SIZE];
    size_t i;
    int result;

    /*
     * Simulate:
     *
     *     6-byte ASCII pairing code
     *     +
     *     64-byte TLS exporter
     */
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

    printf("  BSSL Alice <-> MY Bob: ");

    if (!test_bssl_alice_my_bob(
                password,
                sizeof(password))) {
        printf("FAIL\n");
        result = 0;
    } else {
        printf("OK\n");
    }

    printf("  MY Alice <-> BSSL Bob: ");

    if (!test_my_alice_bssl_bob(
                password,
                sizeof(password))) {
        printf("FAIL\n");
        result = 0;
    } else {
        printf("OK\n");
    }

    if (result) {
        printf("\nPASS\n");
        return 0;
    }

    printf("\nFAIL\n");
    return 1;
}
