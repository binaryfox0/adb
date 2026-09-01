#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <aparse.h>
#include "spake2_sha512.h"

#include <openssl/sha.h>
#include <openssl/rand.h>

#define error aparse_prog_error
#define info aparse_prog_info

#define TEST_COUNT 100000

static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static char *base64_encode(
        const uint8_t *data, 
        const size_t size)
{
    size_t i = 0, j = 0;
    size_t out_len = 0;
    char *encoded = 0;

    out_len = 4 * ((size + 2) / 3);
    encoded = malloc(out_len + 1);
    if (!encoded)
        return NULL;

    while (i < size) 
    {
        unsigned int a = data[i++];
        unsigned int b = (i < size) ? data[i++] : 0;
        unsigned int c = (i < size) ? data[i++] : 0;

        unsigned int triple = (a << 16) | (b << 8) | c;

        encoded[j++] = base64_table[(triple >> 18) & 0x3F];
        encoded[j++] = base64_table[(triple >> 12) & 0x3F];
        encoded[j++] = (i - 1 < size)
                     ? base64_table[(triple >> 6) & 0x3F]
                     : '=';
        encoded[j++] = (i < size + 1)
                     ? base64_table[triple & 0x3F]
                     : '=';
    }

    encoded[j] = '\0';
    return encoded;
}

static void print_data(
        const uint8_t *data,
        const size_t size)
{
    char *base64 = base64_encode(data, size);
    fprintf(stderr, "data = %s", base64);
    free(base64);
}

static void print_digest(
        const char *name,
        const uint8_t *digest)
{
    info(NULL);
    fprintf(stderr, "%s = ", name);
    for(int i = 0; i < SPAKE2__SHA512_DIGEST_LENGTH; i++)
        fprintf(stderr, "%02x", digest[i]);
    fputc('\n', stderr);
}

int main(int argc, char **argv)
{
    int failed = 0;
    aparse_parse(
            argc, argv, 
            NULL, NULL, 
            "SHA512 test");

    for(int i = 0; i < TEST_COUNT; i++)
    {
        uint8_t buf[1024] = {0};
        uint8_t actual[SPAKE2__SHA512_DIGEST_LENGTH] = {0};
        uint8_t expected[SHA512_DIGEST_LENGTH] = {0};

        if(sizeof(expected) != sizeof(actual))
        {
            error("SHA512 digest length mismatch");
            return 1;
        }

        if(RAND_bytes(buf, sizeof(buf)) != 1)
        {
            error("failed to generate random bytes");
            return 1;
        }

        spake2__sha512(buf, sizeof(buf), actual);
        SHA512(buf, sizeof(buf), expected);

        if(memcmp(actual, expected, sizeof(actual)) != 0)
        {
            print_data(buf, sizeof(buf));
            print_digest("    expected", expected);
            print_digest("    actual", actual);
            failed++;
        }
    }
    
    info("summary: %d passed, %d failed", 
            TEST_COUNT - failed, failed);


    return 0;
}
