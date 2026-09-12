#ifndef ADB_AES_H
#define ADB_AES_H

#include <mbedtls/gcm.h>
#include <adb/adb_error.h>

#define ADB__GCM_TAG_LENGTH 16
#define ADB__AES_ENCYPTED_SIZE(x) ((x) + ADB__GCM_TAG_LENGTH)

typedef struct
{
    mbedtls_gcm_context gcm;
    uint64_t enc_seq;
    uint64_t dec_seq;
} adb__aes_t;

adb_error_t adb__aes_init(
        adb__aes_t *aes,
        const uint8_t *key_material,
        const size_t key_material_len);

adb_error_t adb__aes_encrypt(
        adb__aes_t *aes,
        const void *in,
        const size_t in_len,
        void *out,
        const size_t out_len);

adb_error_t adb__aes_decrypt(
        adb__aes_t *aes,
        const void *in,
        const size_t in_len,
        void *out,
        const size_t out_len);

void adb__aes_destroy(
        adb__aes_t *aes);

#endif
