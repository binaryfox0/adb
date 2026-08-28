#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <aparse.h>
#include "spake2_sha512.h"

#define error aparse_prog_error
#define warn aparse_prog_warn
#define info aparse_prog_info

#define SHA512_TEST_MAX_MESSAGE 1024

static char *trim(char *str)
{
    char *end = NULL;

    while(isspace((unsigned char)*str))
        str++;

    end = str + strlen(str);

    while(end > str && isspace((unsigned char)end[-1]))
        end--;

    *end = '\0';

    return str;
}

static int parse_key_value(
        char *line,
        char **key,
        char **value)
{
    char *equals = NULL;

    line = trim(line);

    if(*line == '\0')
        return 0;

    equals = strchr(line, '=');
    if(!equals)
        return 0;

    *equals = '\0';

    *key = trim(line);
    *value = trim(equals + 1);

    if(**key == '[')
    {
        (*key)++;

        *key = trim(*key);

        if((*key)[strlen(*key) - 1] == ']')
            (*key)[strlen(*key) - 1] = '\0';

        *key = trim(*key);
    }

    if(**key == '\0')
        return 0;

    return 1;
}

static int hex_value(const char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';

    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

static int hex_decode(
        const char *str,
        uint8_t *out,
        const size_t out_size,
        size_t *out_len)
{
    size_t len = 0;
    size_t i = 0;

    int hi = 0;
    int lo = 0;

    len = strlen(str);

    if(len & 1)
        return 0;

    if(len / 2 > out_size)
        return 0;

    for(i = 0; i < len; i += 2)
    {
        hi = hex_value(str[i]);
        lo = hex_value(str[i + 1]);

        if(hi < 0 || lo < 0)
            return 0;

        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }

    *out_len = len / 2;

    return 1;
}

static int digest_matches(
        const uint8_t *digest,
        const size_t digest_len,
        const char *expected)
{
    size_t i = 0;

    int hi = 0;
    int lo = 0;

    if(strlen(expected) != digest_len * 2)
        return 0;

    for(i = 0; i < digest_len; i++)
    {
        hi = hex_value(expected[i * 2]);
        lo = hex_value(expected[i * 2 + 1]);

        if(hi < 0 || lo < 0)
            return 0;

        if(digest[i] != (uint8_t)((hi << 4) | lo))
            return 0;
    }

    return 1;
}

static int run_tests(FILE *file)
{
    size_t line_idx = 0;
    char line[4096] = {0};

    char *key = NULL;
    char *value = NULL;

    uint8_t message[SHA512_TEST_MAX_MESSAGE] = {0};
    uint8_t digest[SPAKE2__SHA512_DIGEST_LEN] = {0};

    size_t msg_len = 0;
    size_t expected_msg_len = 0;

    size_t tests = 0;
    size_t passed = 0;
    size_t failed = 0;

    unsigned long digest_len = 0;

    while(fgets(line, sizeof(line), file))
    {
        line_idx++;
        if(!parse_key_value(line, &key, &value))
            continue;

        if(strcmp(key, "L") == 0)
        {
            digest_len = strtoul(value, NULL, 10);
            if(digest_len != SPAKE2__SHA512_DIGEST_LEN)
            {
                error("unexpected digest length: %lu", digest_len);
                return 1;
            }
            continue;
        }

        if(strcmp(key, "Len") == 0)
        {
            unsigned long length_bits = strtoul(value, NULL, 10);
            if(length_bits & 7)
            {
                error("message length is not byte aligned");
                return 0;
            }

            expected_msg_len = length_bits / 8;
            if(expected_msg_len > sizeof(message))
            {
                error("message is too large: %zu bytes",
                    expected_msg_len);
                return 0;
            }
            continue;
        }

        if(strcmp(key, "Msg") == 0)
        {
            if(!hex_decode(
                    value,
                    message,
                    sizeof(message),
                    &msg_len))
            {
                error("invalid hexadecimal message");
                return 0;
            }

            if(expected_msg_len == 0) 
            {
                msg_len = 0;
                continue;
            }

            if(msg_len != expected_msg_len)
            {
                error("message length mismatch: expected %zu, got %zu",
                    expected_msg_len, msg_len);
                return 1;
            }

            continue;
        }

        if(strcmp(key, "MD") == 0)
        {
            tests++;

            memset(digest, 0, sizeof(digest));
            spake2__sha512(message, msg_len, digest);
            if(digest_matches(digest, sizeof(digest),
                        value))
                passed++;
            else
            {
                failed++;
                error("FAIL: test %zu, len: %zu bytes, line %zu",
                    tests, msg_len, line_idx);
            }

            continue;
        }
    }

    info("summary: %zu tests, %zu passed, %zu failed",
        tests, passed, failed);

    return failed == 0;
}

int main(int argc, char **argv)
{
    FILE *file = NULL;
    const char *path = NULL;

    aparse_arg main_args[] =
    {
        aparse_arg_string(
                "path",
                &path, 0,
                "Path to SHA512ShortMsg.rsp"),
        aparse_arg_end_marker
    };

    if(aparse_parse(
                argc, argv,
                main_args, NULL,
                "SHA512 tests") != APARSE_STATUS_OK)
        return 1;

    file = fopen(path, "r");
    if(!file)
    {
        error("failed to open \"%s\"", path);
        info("reason: %s", strerror(errno));
        return 1;
    }

    if(!run_tests(file))
    {
        fclose(file);
        return 1;
    }

    fclose(file);
    return 0;
}
