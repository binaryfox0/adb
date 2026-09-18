#ifndef ADB_KEY_PRIV_H
#define ADB_KEY_PRIV_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <adb/adb_error.h>

#define ADB__RSA_MODULUS_BITS   2048
#define ADB__KEY_SIGNATURE_LENGTH (ADB__RSA_MODULUS_BITS / 8)

typedef struct adb_key adb_key_t;
typedef struct adb_ctx adb_ctx_t;
typedef struct mbedtls_x509_crt mbedtls_x509_crt;
typedef struct mbedtls_pk_context mbedtls_pk_context;

// bool adb__key_write_x509_pem(
//         adb_key_t *key,
//         adb_ctx_t *ctx,
//         uint8_t *out,
//         const size_t out_size);

mbedtls_pk_context *adb__key_get_pk(
        adb_key_t *key);

bool adb__key_create_x509(
        adb_key_t *key,
        adb_ctx_t *ctx,
        mbedtls_x509_crt *crt);

adb_error_t adb__key_sign(
        adb_key_t *key,
        adb_ctx_t *ctx,
        const char *token,
        const size_t token_size,
        uint8_t *out_sig);

#endif
