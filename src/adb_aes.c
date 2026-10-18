#include "adb_aes.h"

#include <string.h>

#define ADB__AES_KEY_LENGTH 16U
#define ADB__GCM_NONCE_LENGTH 12U

adb_error_t adb__aes_init(
        adb__aes_t *aes,
        const uint8_t *key_material,
        const size_t key_material_len)
{
    static const uint8_t info[] = "adb pairing_auth aes-128-gcm key";

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t status = PSA_SUCCESS;

    if (!aes || !key_material || key_material_len == 0U)
        return ADB_ERR_PARAM;

    *aes = (adb__aes_t){0};

    status = psa_crypto_init();
    if (status != PSA_SUCCESS)
        return ADB_ERR_CRYPTO;

    status = psa_key_derivation_setup(&operation, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (status != PSA_SUCCESS)
        goto cleanup;

    status = psa_key_derivation_input_bytes(
            &operation, PSA_KEY_DERIVATION_INPUT_SECRET,
            key_material, key_material_len);
    if (status != PSA_SUCCESS)
        goto cleanup;

    status = psa_key_derivation_input_bytes(
            &operation, PSA_KEY_DERIVATION_INPUT_INFO,
            info, sizeof(info) - 1U);
    if (status != PSA_SUCCESS)
        goto cleanup;

    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, ADB__AES_KEY_LENGTH * 8U);
    psa_set_key_usage_flags(&attributes,
            PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_GCM);

    status = psa_key_derivation_output_key(
            &attributes, &operation, &aes->key_id);

cleanup:
    psa_key_derivation_abort(&operation);
    psa_reset_key_attributes(&attributes);

    if (status != PSA_SUCCESS)
    {
        if (aes->key_id != PSA_KEY_ID_NULL)
            psa_destroy_key(aes->key_id);
        *aes = (adb__aes_t){0};
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
    uint8_t nonce[ADB__GCM_NONCE_LENGTH] = {0};
    size_t output_length = 0U;
    psa_status_t status = PSA_SUCCESS;

    if (!aes || !in || !out)
        return ADB_ERR_PARAM;

    if (in_len > SIZE_MAX - ADB__GCM_TAG_LENGTH ||
            out_len < ADB__AES_ENCRYPTED_SIZE(in_len))
        return ADB_ERR_TOO_SMALL;

    if (aes->enc_seq == UINT64_MAX)
        return ADB_ERR_CRYPTO;

    memcpy(nonce, &aes->enc_seq, sizeof(aes->enc_seq));

    status = psa_aead_encrypt(
            aes->key_id, PSA_ALG_GCM,
            nonce, sizeof(nonce),
            NULL, 0U,
            in, in_len,
            out, out_len,
            &output_length);

    if (status != PSA_SUCCESS ||
            output_length != ADB__AES_ENCRYPTED_SIZE(in_len))
        return ADB_ERR_CRYPTO;

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
    uint8_t nonce[ADB__GCM_NONCE_LENGTH] = {0};
    size_t output_length = 0U;
    size_t ciphertext_length = 0U;
    psa_status_t status = PSA_SUCCESS;

    if (!aes || !in || in_len < ADB__GCM_TAG_LENGTH || !out)
        return ADB_ERR_PARAM;

    ciphertext_length = in_len - ADB__GCM_TAG_LENGTH;
    if (out_len < ciphertext_length)
        return ADB_ERR_TOO_SMALL;

    if (aes->dec_seq == UINT64_MAX)
        return ADB_ERR_CRYPTO;

    memcpy(nonce, &aes->dec_seq, sizeof(aes->dec_seq));

    status = psa_aead_decrypt(
            aes->key_id, PSA_ALG_GCM,
            nonce, sizeof(nonce),
            NULL, 0U,
            in, in_len,
            out, out_len,
            &output_length);

    if (status != PSA_SUCCESS || output_length != ciphertext_length)
        return ADB_ERR_CRYPTO;

    aes->dec_seq++;
    return ADB_ERR_OK;
}

void adb__aes_destroy(adb__aes_t *aes)
{
    if (!aes)
        return;

    if (aes->key_id != PSA_KEY_ID_NULL)
        psa_destroy_key(aes->key_id);

    *aes = (adb__aes_t){0};
}