#include "adb_aes.h"

#include <string.h>
#include <mbedtls/hkdf.h>
#include "adb_log_priv.h"

#define ADB__AES_KEY_LENGTH 16
#define ADB__GCM_NONCE_LENGTH 12
adb_error_t adb__aes_init(
        adb__aes_t *aes,
        const uint8_t *key_material,
        const size_t key_material_len)
{
    static const uint8_t info[] = "adb pairing_auth aes-128-gcm key";

    int err = 0;
    uint8_t key[ADB__AES_KEY_LENGTH] = {0};
    if(!aes || !key_material || key_material_len == 0)
        return ADB_ERR_PARAM;

    aes->enc_seq = 0;
    aes->dec_seq = 0;

    err = mbedtls_hkdf(
            mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
            NULL,
            0,
            key_material,
            key_material_len,
            info,
            sizeof(info) - 1,
            key,
            sizeof(key));
    if(err != 0)
    {
        adb__log_err_mbedtls(err, 
                "failed to derieve AES-128 key from key material");
        return ADB_ERR_CRYPTO;
    }

    mbedtls_gcm_init(&aes->gcm);
    err = mbedtls_gcm_setkey(
            &aes->gcm, 
            MBEDTLS_CIPHER_ID_AES, 
            key, 
            128);
    if(err != 0)
    {
        adb__log_err_mbedtls(err, "failed to set key to GCM context");
        mbedtls_gcm_free(&aes->gcm);
        return ADB_ERR_CRYPTO;
    }

    return ADB_ERR_OK;
}

adb_error_t adb__aes_encrypt(
        adb__aes_t *aes,
        const void *in,
        const size_t in_len,
        void *out,
        const size_t out_len)
{
    int err = 0;
    uint8_t nonce[ADB__GCM_NONCE_LENGTH] = {0};
    if(!aes || !in || !out)
        return ADB_ERR_PARAM;
    
    if(out_len < ADB__AES_ENCYPTED_SIZE(in_len))
        return ADB_ERR_TOO_SMALL;

    memcpy(nonce, &aes->enc_seq, sizeof(aes->enc_seq));
    err = mbedtls_gcm_crypt_and_tag(
            &aes->gcm,
            MBEDTLS_GCM_ENCRYPT,
            in_len,
            nonce,
            sizeof(nonce),
            NULL,
            0,
            in,
            out,
            ADB__GCM_TAG_LENGTH,
            (uint8_t*)out + in_len);
    if(err != 0)
    {
        adb__log_err_mbedtls(err, "failed to encrypt data");
        return ADB_ERR_CRYPTO;
    }
            
    aes->enc_seq++;
    return ADB_ERR_OK;
}

adb_error_t adb__aes_decrypt(
        adb__aes_t *aes,
        const void *in,
        const size_t in_len,
        void *out,
        const size_t out_len)
{
    int err = 0;
    size_t ciphertext_len = 0;
    uint8_t nonce[ADB__GCM_NONCE_LENGTH] = {0};

    if(!aes || !in || in_len < ADB__GCM_TAG_LENGTH || !out)
        return ADB_ERR_PARAM;
    if(out_len < in_len - ADB__GCM_TAG_LENGTH)
        return ADB_ERR_TOO_SMALL;

    ciphertext_len = in_len - ADB__GCM_TAG_LENGTH;
    memcpy(nonce, &aes->dec_seq, sizeof(aes->dec_seq));
    err = mbedtls_gcm_auth_decrypt(
            &aes->gcm,
            ciphertext_len,
            nonce,
            sizeof(nonce),
            NULL,
            0,
            (const uint8_t*)in + ciphertext_len,
            ADB__GCM_TAG_LENGTH,
            in,
            out);
    if(err != 0)
    {
        adb__log_err_mbedtls(err, "failed to decrypt data");
        return ADB_ERR_CRYPTO;
    }

    aes->dec_seq++;
    return ADB_ERR_OK;
}

void adb__aes_destroy(
        adb__aes_t *aes)
{
    if(!aes)
        return;
    mbedtls_gcm_free(&aes->gcm);
    *aes = (adb__aes_t){0};
}
