#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "spake2.h"

#define TEST_PASSWORD      "123456"
#define TEST_ALICE_NAME    "alice"
#define TEST_BOB_NAME      "bob"

#define TEST_MSG_MAX       1024
#define TEST_KEY_MAX       1024

static void test_spake2_alice_bob(void)
{
    spake2_ctx_t *alice;
    spake2_ctx_t *bob;

    uint8_t alice_random[SPAKE2_RANDOM_DATA_LENGTH];
    uint8_t bob_random[SPAKE2_RANDOM_DATA_LENGTH];

    uint8_t alice_msg[TEST_MSG_MAX];
    uint8_t bob_msg[TEST_MSG_MAX];

    uint8_t alice_key[TEST_KEY_MAX];
    uint8_t bob_key[TEST_KEY_MAX];

    size_t alice_msg_len;
    size_t bob_msg_len;
    size_t alice_key_len;
    size_t bob_key_len;

    const uint8_t *password;
    size_t password_len;

    size_t i;

    alice = NULL;
    bob = NULL;

    alice_msg_len = 0;
    bob_msg_len = 0;
    alice_key_len = 0;
    bob_key_len = 0;

    password = (const uint8_t *)TEST_PASSWORD;
    password_len = sizeof(TEST_PASSWORD) - 1;

    /*
     * Fixed randomness makes the test completely deterministic.
     *
     * Alice and Bob MUST use different random values.
     */
    for (i = 0; i < sizeof(alice_random); ++i) {
        alice_random[i] = (uint8_t)i;
        bob_random[i] = (uint8_t)(0x80u + i);
    }

    alice = spake2_ctx_new(
        NULL,
        SPAKE2_ROLE_ALICE,
        (const uint8_t *)TEST_ALICE_NAME,
        sizeof(TEST_ALICE_NAME) - 1,
        (const uint8_t *)TEST_BOB_NAME,
        sizeof(TEST_BOB_NAME) - 1);

    assert(alice != NULL);

    bob = spake2_ctx_new(
        NULL,
        SPAKE2_ROLE_BOB,
        (const uint8_t *)TEST_BOB_NAME,
        sizeof(TEST_BOB_NAME) - 1,
        (const uint8_t *)TEST_ALICE_NAME,
        sizeof(TEST_ALICE_NAME) - 1);

    assert(bob != NULL);

    /*
     * Generate Alice's SPAKE2 message.
     */
    assert(spake2_generate_msg(
        alice,
        alice_msg,
        &alice_msg_len,
        sizeof(alice_msg),
        password,
        password_len,
        alice_random));

    /*
     * Generate Bob's SPAKE2 message.
     */
    assert(spake2_generate_msg(
        bob,
        bob_msg,
        &bob_msg_len,
        sizeof(bob_msg),
        password,
        password_len,
        bob_random));

    assert(alice_msg_len != 0);
    assert(bob_msg_len != 0);

    /*
     * Exchange messages and derive the shared key.
     */
    assert(spake2_process_msg(
        alice,
        alice_key,
        &alice_key_len,
        sizeof(alice_key),
        bob_msg,
        bob_msg_len));

    assert(spake2_process_msg(
        bob,
        bob_key,
        &bob_key_len,
        sizeof(bob_key),
        alice_msg,
        alice_msg_len));

    /*
     * Both parties MUST derive exactly the same key.
     */
    assert(alice_key_len == bob_key_len);
    assert(alice_key_len != 0);

    assert(memcmp(
        alice_key,
        bob_key,
        alice_key_len) == 0);

    printf("SPAKE2 Alice/Bob test: PASS\n");
    printf("message length: Alice=%zu Bob=%zu\n",
           alice_msg_len, bob_msg_len);
    printf("key length: %zu\n", alice_key_len);

    spake2_ctx_free(alice);
    spake2_ctx_free(bob);
}

int main(void)
{
    test_spake2_alice_bob();

    return 0;
}
