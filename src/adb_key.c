#include <adb/adb_key.h>
#include "adb/adb_ctx.h"
#include "adb_key_priv.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>

#include <psa/crypto.h>
#include <mbedtls/pk.h>
#include <mbedtls/base64.h>
#include <mbedtls/asn1write.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pem.h>
#include <mbedtls/x509.h>
#include <mbedtls/md.h>
#include <mbedtls/oid.h>

#include "adb_alloc_priv.h"
#include "adb_ctx_priv.h"
#include "adb_log_priv.h"
#include "adb_utils.h"

#define ADB__CERT_LIFETIME (10 * 365 * 24 * 60 * 60)
#define ADB__RSA_ALGORITHM "RSA-" ADB__STRINGIFY(ADB__RSA_MODULUS_BITS)
#define ADB__RSA_MODULUS_SIZE (ADB__RSA_MODULUS_BITS / 8U)
#define ADB__RSA_MODULUS_WORDS (ADB__RSA_MODULUS_SIZE / 4U)
#define ADB__ANDROID_PUBKEY_SIZE \
    (3U * sizeof(uint32_t) + 2U * ADB__RSA_MODULUS_SIZE)
#define ADB__BASE64_SIZE(n) (4U * (((n) + 2U) / 3U))
#define ADB__RSA2048_DER_MAX 2048U
#define ADB__TOKEN_SIZE 20U

typedef struct
{
    uint32_t modulus_size_words;
    uint32_t n0inv;
    uint8_t modulus[ADB__RSA_MODULUS_SIZE];
    uint8_t rr[ADB__RSA_MODULUS_SIZE];
    uint32_t exponent;
} adb__android_pubkey_t;

typedef struct adb_key
{
    mbedtls_pk_context pk;
    mbedtls_svc_key_id_t psa_key_id;
    bool owns_psa_key;
    char *pubkey;
} adb_key_t;

_Static_assert(ADB__RSA_MODULUS_BITS % 8U == 0U,
        "RSA modulus size must be a whole number of bytes");
_Static_assert(ADB__RSA_MODULUS_SIZE % 4U == 0U,
        "RSA modulus size must be a whole number of words");
_Static_assert(sizeof(adb__android_pubkey_t) == ADB__ANDROID_PUBKEY_SIZE,
        "adb__android_pubkey must be exactly the expected size");

static void adb__store_u32_le(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static bool adb__der_read_tlv(
        const uint8_t *buffer,
        const size_t buffer_size,
        size_t *offset,
        const uint8_t expected_tag,
        const uint8_t **value,
        size_t *value_size)
{
    bool ok = false;
    size_t pos = 0U;
    size_t len = 0U;
    size_t len_bytes = 0U;
    uint8_t first_len = 0U;

    if (!buffer || !offset || !value || !value_size || *offset >= buffer_size)
        return false;

    pos = *offset;
    if (buffer[pos++] != expected_tag || pos >= buffer_size)
        return false;

    first_len = buffer[pos++];
    if ((first_len & 0x80U) == 0U)
    {
        len = first_len;
    }
    else
    {
        len_bytes = (size_t)(first_len & 0x7FU);
        if (len_bytes == 0U || len_bytes > sizeof(size_t) ||
                len_bytes > buffer_size - pos || buffer[pos] == 0U)
            return false;

        for (size_t i = 0U; i < len_bytes; ++i)
        {
            if (len > (SIZE_MAX >> 8))
                return false;
            len = (len << 8) | buffer[pos + i];
        }
        if (len < 128U)
            return false;
        pos += len_bytes;
    }

    if (len > buffer_size - pos)
        return false;

    *value = buffer + pos;
    *value_size = len;
    *offset = pos + len;
    ok = true;
    return ok;
}

static int adb__compare_le(
        const uint8_t *a,
        const uint8_t *b,
        const size_t size)
{
    int result = 0;

    for (size_t i = size; i > 0U; --i)
    {
        if (a[i - 1U] > b[i - 1U])
        {
            result = 1;
            break;
        }
        if (a[i - 1U] < b[i - 1U])
        {
            result = -1;
            break;
        }
    }
    return result;
}

static uint32_t adb__rsa_n0inv(const uint8_t *modulus_le)
{
    uint32_t low_word = 0U;
    uint32_t inverse = 1U;

    low_word = (uint32_t)modulus_le[0] |
            ((uint32_t)modulus_le[1] << 8) |
            ((uint32_t)modulus_le[2] << 16) |
            ((uint32_t)modulus_le[3] << 24);

    /* Newton iteration computes the inverse modulo 2^32. */
    for (size_t i = 0U; i < 5U; ++i)
        inverse *= 2U - low_word * inverse;

    return 0U - inverse;
}

static bool adb__rsa_calculate_rr(
        uint8_t *rr,
        const uint8_t *modulus_le)
{
    uint8_t value[ADB__RSA_MODULUS_SIZE + 1U] = {0};
    size_t modulus_size = ADB__RSA_MODULUS_SIZE;
    bool greater_or_equal = false;
    unsigned int carry = 0U;
    unsigned int borrow = 0U;

    if (!rr || !modulus_le || (modulus_le[0] & 1U) == 0U)
        return false;

    /* Android wants R^2 mod N, where R = 2^(RSA modulus bit size). */
    value[0] = 1U;
    for (size_t round = 0U;
            round < (size_t)ADB__RSA_MODULUS_BITS * 2U;
            ++round)
    {
        carry = 0U;
        for (size_t i = 0U; i < modulus_size; ++i)
        {
            unsigned int doubled = ((unsigned int)value[i] << 1) | carry;
            value[i] = (uint8_t)doubled;
            carry = doubled >> 8;
        }
        value[modulus_size] = (uint8_t)carry;

        greater_or_equal = value[modulus_size] != 0U;
        if (!greater_or_equal)
            greater_or_equal = adb__compare_le(value, modulus_le,
                    modulus_size) >= 0;

        if (greater_or_equal)
        {
            borrow = 0U;
            for (size_t i = 0U; i <= modulus_size; ++i)
            {
                unsigned int modulus_byte =
                        i < modulus_size ? modulus_le[i] : 0U;
                unsigned int subtrahend = modulus_byte + borrow;
                unsigned int value_byte = value[i];
                value[i] = (uint8_t)(value_byte - subtrahend);
                borrow = value_byte < subtrahend ? 1U : 0U;
            }
            if (borrow != 0U)
                return false;
        }
    }

    if (value[modulus_size] != 0U)
        return false;
    memcpy(rr, value, modulus_size);
    return true;
}

adb_error_t adb_key_generate(adb_key_t **key, adb_ctx_t *ctx)
{
    adb_error_t ret = ADB_ERR_CRYPTO;
    adb_key_t *tmp = NULL;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t status = PSA_SUCCESS;
    int err = 0;

    if (!key || !ctx)
        return ADB_ERR_PARAM;
    *key = NULL;

    ADB__INFO("generating new RSA key pair ("
            ADB__STRINGIFY(ADB__RSA_MODULUS_BITS) " bits)");

    status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        ADB__ERROR("PSA crypto initialization failed (status %d)",
                (int)status);
        return ADB_ERR_CRYPTO;
    }

    tmp = adb__calloc(1U, sizeof(*tmp));
    if (!tmp)
        return ADB_ERR_NO_MEM;

    mbedtls_pk_init(&tmp->pk);

    psa_set_key_type(&attributes, PSA_KEY_TYPE_RSA_KEY_PAIR);
    psa_set_key_bits(&attributes, (size_t)ADB__RSA_MODULUS_BITS);
    psa_set_key_usage_flags(&attributes,
            PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_EXPORT);
    psa_set_key_algorithm(&attributes,
            PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_ANY_HASH));

    status = psa_generate_key(&attributes, &tmp->psa_key_id);
    psa_reset_key_attributes(&attributes);
    if (status != PSA_SUCCESS)
    {
        ADB__ERROR("failed to generate " ADB__RSA_ALGORITHM
                " key pair (PSA status %d)", (int)status);
        goto cleanup;
    }
    tmp->owns_psa_key = true;

    err = mbedtls_pk_wrap_psa(&tmp->pk, tmp->psa_key_id);
    if (err != 0)
    {
        ADB__ERROR("failed to wrap PSA RSA key (status %d)", err);
        goto cleanup;
    }

    ADB__INFO(ADB__RSA_ALGORITHM " key pair generated successfully");
    *key = tmp;
    return ADB_ERR_OK;

cleanup:
    adb_key_destroy(tmp);
    return ret;
}

adb_error_t adb_key_load(adb_key_t **key, adb_ctx_t *ctx, const char *path)
{
    adb_error_t ret = ADB_ERR_OK;
    adb_key_t *tmp = NULL;
    char *buffer = NULL;
    size_t file_size = 0U;
    psa_status_t status = PSA_SUCCESS;
    int err = 0;

    if (!key || !ctx || !path)
        return ADB_ERR_PARAM;
    *key = NULL;

    ADB__INFO("loading key from \"%s\"", path);

    status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        ADB__ERROR("PSA crypto initialization failed (status %d)",
                (int)status);
        return ADB_ERR_CRYPTO;
    }

    tmp = adb__calloc(1U, sizeof(*tmp));
    if (!tmp)
        return ADB_ERR_NO_MEM;

    mbedtls_pk_init(&tmp->pk);

    ret = adb__util_read_file(path, &buffer, &file_size);
    if (ret != ADB_ERR_OK)
    {
        adb__log_err_errno("failed to load key at \"%s\"", path);
        goto cleanup;
    }
    if (file_size == SIZE_MAX)
    {
        ret = ADB_ERR_TOO_LONG;
        goto cleanup;
    }

    err = mbedtls_pk_parse_key(&tmp->pk,
            (const uint8_t *)buffer, file_size + 1U, NULL, 0U);
    if (err != 0)
    {
        adb__log_err_mbedtls(err,
                "failed to process key from \"%s\"", path);
        ret = ADB_ERR_CRYPTO;
        goto cleanup;
    }

    if (!mbedtls_pk_can_do_psa(&tmp->pk,
                PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_1),
                PSA_KEY_USAGE_SIGN_HASH) ||
            mbedtls_pk_get_bitlen(&tmp->pk) != ADB__RSA_MODULUS_BITS)
    {
        ADB__ERROR("the given key was not " ADB__RSA_ALGORITHM ": \"%s\"",
                path);
        ret = ADB_ERR_UNSUPPORTED;
        goto cleanup;
    }

    *key = tmp;
    tmp = NULL;
    ADB__INFO("loaded key successfully from \"%s\"", path);

cleanup:
    if (tmp)
        adb_key_destroy(tmp);
    adb__free(buffer);
    return ret;
}

static adb_error_t adb__key_write_pkcs8_pem(
        adb_key_t *key,
        uint8_t *out,
        const size_t out_size,
        size_t *written)
{
    uint8_t der[ADB__RSA2048_DER_MAX] = {0};
    uint8_t *end = NULL;
    uint8_t *p = NULL;
    uint8_t *alg_end = NULL;
    size_t pkcs1_len = 0U;
    size_t alg_id_len = 0U;
    size_t content_len = 0U;
    size_t der_len = 0U;
    size_t pem_len = 0U;
    int err = 0;

    if (!key || !out || out_size == 0U)
        return ADB_ERR_PARAM;

    end = der + sizeof(der);
    err = mbedtls_pk_write_key_der(&key->pk, der, sizeof(der));
    if (err < 0)
        goto fail;

    pkcs1_len = (size_t)err;
    if (pkcs1_len > sizeof(der))
        goto fail;
    p = end - pkcs1_len;

    /* privateKey OCTET STRING */
    if ((err = mbedtls_asn1_write_len(&p, der, pkcs1_len)) < 0 ||
            (err = mbedtls_asn1_write_tag(
                    &p, der, MBEDTLS_ASN1_OCTET_STRING)) < 0)
        goto fail;

    /* Everything after this point belongs to AlgorithmIdentifier. */
    alg_end = p;

    /* AlgorithmIdentifier ::= SEQUENCE { rsaEncryption, NULL } */
    if ((err = mbedtls_asn1_write_null(&p, der)) < 0 ||
            (err = mbedtls_asn1_write_oid(&p, der,
                    MBEDTLS_OID_PKCS1 "\x01",
                    sizeof(MBEDTLS_OID_PKCS1 "\x01") - 1U)) < 0)
        goto fail;

    alg_id_len = (size_t)(alg_end - p);
    if ((err = mbedtls_asn1_write_len(&p, der, alg_id_len)) < 0 ||
            (err = mbedtls_asn1_write_tag(&p, der,
                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) < 0)
        goto fail;

    /* PrivateKeyInfo version INTEGER 0. */
    err = mbedtls_asn1_write_int(&p, der, 0);
    if (err < 0)
        goto fail;

    content_len = (size_t)(end - p);
    if ((err = mbedtls_asn1_write_len(&p, der, content_len)) < 0 ||
            (err = mbedtls_asn1_write_tag(&p, der,
                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) < 0)
        goto fail;

    der_len = (size_t)(end - p);
    err = mbedtls_pem_write_buffer(
            "-----BEGIN PRIVATE KEY-----\n",
            "-----END PRIVATE KEY-----\n",
            p, der_len, out, out_size, &pem_len);
    if (err != 0)
        goto fail;

    if (written)
        *written = pem_len;
    return ADB_ERR_OK;

fail:
    adb__log_err_mbedtls(err, "failed to write PKCS#8 PEM from key");
    if (err == MBEDTLS_ERR_ASN1_BUF_TOO_SMALL ||
            err == PSA_ERROR_BUFFER_TOO_SMALL ||
            err == MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL)
        return ADB_ERR_TOO_SMALL;
    return ADB_ERR_CRYPTO;
}

adb_error_t adb_key_save(adb_key_t *key, const char *path)
{
    adb_error_t ret = ADB_ERR_OK;
    uint8_t buffer[4096] = {0};
    size_t written = 0U;

    if (!key || !path)
        return ADB_ERR_PARAM;

    ADB__INFO("saving key to \"%s\"", path);
    ret = adb__key_write_pkcs8_pem(key, buffer, sizeof(buffer), &written);
    if (ret != ADB_ERR_OK)
        return ret;
    if (written == 0U)
        return ADB_ERR_CRYPTO;

    ret = adb__util_write_file(path, buffer, written - 1U);
    if (ret != ADB_ERR_OK)
        return ret;

    ADB__INFO("saved key to \"%s\" successfully", path);
    return ADB_ERR_OK;
}

static bool adb__encode_android_pubkey(
        const mbedtls_pk_context *pk,
        adb__android_pubkey_t *out)
{
    bool ret = false;
    uint8_t der[ADB__RSA2048_DER_MAX] = {0};
    const uint8_t *der_data = NULL;
    const uint8_t *sequence = NULL;
    const uint8_t *algorithm = NULL;
    const uint8_t *bit_string = NULL;
    const uint8_t *rsa_sequence = NULL;
    const uint8_t *modulus = NULL;
    const uint8_t *exponent_data = NULL;
    size_t der_len = 0U;
    size_t offset = 0U;
    size_t sequence_len = 0U;
    size_t sequence_offset = 0U;
    size_t algorithm_len = 0U;
    size_t bit_string_len = 0U;
    size_t rsa_sequence_len = 0U;
    size_t rsa_offset = 0U;
    size_t modulus_len = 0U;
    size_t exponent_len = 0U;
    uint32_t exponent = 0U;
    int err = 0;

    if (!pk || !out)
        return false;

    *out = (adb__android_pubkey_t){0};
    ADB__INFO("encoding Android public key from private key");

    if (!mbedtls_pk_can_do_psa(pk,
                PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_1),
                PSA_KEY_USAGE_SIGN_HASH) ||
            mbedtls_pk_get_bitlen(pk) != ADB__RSA_MODULUS_BITS)
        return false;

    err = mbedtls_pk_write_pubkey_der(pk, der, sizeof(der));
    if (err < 0)
        goto cleanup;
    der_len = (size_t)err;
    if (der_len > sizeof(der))
        goto cleanup;
    der_data = der + sizeof(der) - der_len;
    err = 0;

    /* SubjectPublicKeyInfo contains an RSA public key in a BIT STRING. */
    if (!adb__der_read_tlv(der_data, der_len, &offset, 0x30U,
            &sequence, &sequence_len) || offset != der_len)
        goto cleanup;

    if (!adb__der_read_tlv(sequence, sequence_len, &sequence_offset, 0x30U,
            &algorithm, &algorithm_len) ||
            !adb__der_read_tlv(sequence, sequence_len, &sequence_offset, 0x03U,
                    &bit_string, &bit_string_len) ||
            sequence_offset != sequence_len || algorithm_len == 0U ||
            bit_string_len < 2U || bit_string[0] != 0U)
        goto cleanup;

    offset = 0U;
    if (!adb__der_read_tlv(bit_string + 1U, bit_string_len - 1U, &offset,
            0x30U, &rsa_sequence, &rsa_sequence_len) ||
            offset != bit_string_len - 1U)
        goto cleanup;

    if (!adb__der_read_tlv(rsa_sequence, rsa_sequence_len, &rsa_offset, 0x02U,
            &modulus, &modulus_len) ||
            !adb__der_read_tlv(rsa_sequence, rsa_sequence_len, &rsa_offset, 0x02U,
                    &exponent_data, &exponent_len) ||
            rsa_offset != rsa_sequence_len)
        goto cleanup;

    /* DER INTEGERs are big endian and may have a sign-protection zero byte. */
    while (modulus_len > ADB__RSA_MODULUS_SIZE && modulus_len > 0U &&
            modulus[0] == 0U)
    {
        ++modulus;
        --modulus_len;
    }
    if (modulus_len != ADB__RSA_MODULUS_SIZE)
        goto cleanup;

    while (exponent_len > 1U && exponent_data[0] == 0U)
    {
        ++exponent_data;
        --exponent_len;
    }
    if (exponent_len == 0U || exponent_len > sizeof(exponent))
        goto cleanup;
    for (size_t i = 0U; i < exponent_len; ++i)
        exponent = (exponent << 8) | exponent_data[i];

    for (size_t i = 0U; i < ADB__RSA_MODULUS_SIZE; ++i)
        out->modulus[i] = modulus[ADB__RSA_MODULUS_SIZE - 1U - i];

    if (!adb__rsa_calculate_rr(out->rr, out->modulus))
        goto cleanup;

    adb__store_u32_le((uint8_t *)&out->modulus_size_words,
            (uint32_t)ADB__RSA_MODULUS_WORDS);
    adb__store_u32_le((uint8_t *)&out->n0inv,
            adb__rsa_n0inv(out->modulus));
    adb__store_u32_le((uint8_t *)&out->exponent, exponent);
    ret = true;
    ADB__INFO("encoded Android public key successfully");

cleanup:
    if (!ret)
    {
        if (err != 0)
            adb__log_err_mbedtls(err,
                    "failed to export RSA public key for Android encoding");
        else
            ADB__ERROR("failed to encode Android RSA public key");
    }
    return ret;
}

adb_error_t adb_key_generate_pubkey(
        adb_key_t *key,
        uint8_t *buffer,
        const size_t size,
        size_t *out_size)
{
    int err = 0;
    adb__android_pubkey_t pubkey = {0};
    size_t encoded_size = ADB__BASE64_SIZE(ADB__ANDROID_PUBKEY_SIZE);
    struct passwd *pw = NULL;
    char hostname[HOST_NAME_MAX + 1U] = {0};
    size_t min_size = 0U;

    if (!key || ((!buffer) != (size == 0U)) || (!buffer && !out_size))
        return ADB_ERR_PARAM;

    ADB__INFO("generating public key from private key");
    if (!adb__encode_android_pubkey(&key->pk, &pubkey))
        return ADB_ERR_CRYPTO;

    pw = getpwuid(getuid());
    if (!pw || !pw->pw_name)
    {
        adb__log_err_errno("failed to get current user name");
        return ADB_ERR_GENERIC;
    }
    if (gethostname(hostname, sizeof(hostname)) != 0)
    {
        adb__log_err_errno("failed to get host name");
        return ADB_ERR_GENERIC;
    }
    hostname[sizeof(hostname) - 1U] = '\0';

    min_size = encoded_size + 1U + strlen(pw->pw_name) + 1U +
            strlen(hostname) + 1U;
    if (out_size)
        *out_size = min_size;
    if (!buffer)
        return ADB_ERR_OK;
    if (min_size > size)
        return ADB_ERR_TOO_SMALL;

    err = mbedtls_base64_encode(buffer, size, &encoded_size,
            (const uint8_t *)&pubkey, ADB__ANDROID_PUBKEY_SIZE);
    if (err != 0)
    {
        adb__log_err_mbedtls(err, "failed to encode public key");
        return ADB_ERR_CRYPTO;
    }

    err = snprintf((char *)buffer + encoded_size, size - encoded_size,
            " %s@%s", pw->pw_name, hostname);
    if (err < 0 || (size_t)err >= size - encoded_size)
        return ADB_ERR_TOO_SMALL;

    ADB__INFO("generated public key successfully");
    return ADB_ERR_OK;
}

void adb_key_destroy(adb_key_t *key)
{
    psa_status_t status = PSA_SUCCESS;

    if (!key)
        return;

    mbedtls_pk_free(&key->pk);
    if (key->owns_psa_key)
    {
        status = psa_destroy_key(key->psa_key_id);
        if (status != PSA_SUCCESS)
            ADB__ERROR("failed to destroy PSA key (status %d)", (int)status);
    }

    adb__free(key->pubkey);
    adb__free(key);
}

static int adb__key_x509write_cert(
        adb_key_t *key,
        mbedtls_x509write_cert *writer)
{
    int err = 0;
    time_t now_time = 0;
    time_t after_time = 0;
    char not_before[16] = {0};
    char not_after[16] = {0};
    struct tm tm_now = {0};
    struct tm tm_after = {0};

    if (!key || !writer)
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;

    mbedtls_x509write_crt_set_version(writer, MBEDTLS_X509_CRT_VERSION_3);
    now_time = time(NULL);
    if (now_time == (time_t)-1)
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;
    after_time = now_time + (time_t)ADB__CERT_LIFETIME;

    if (gmtime_r(&now_time, &tm_now) == NULL ||
            gmtime_r(&after_time, &tm_after) == NULL)
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;

    if (strftime(not_before, sizeof(not_before), "%Y%m%d%H%M%S", &tm_now) == 0U ||
            strftime(not_after, sizeof(not_after), "%Y%m%d%H%M%S", &tm_after) == 0U)
        return MBEDTLS_ERR_X509_BAD_INPUT_DATA;

    err = mbedtls_x509write_crt_set_validity(writer, not_before, not_after);
    if (err != 0)
        goto fail;
    err = mbedtls_x509write_crt_set_subject_name(writer, "C=US,O=Android,CN=Adb");
    if (err != 0)
        goto fail;
    err = mbedtls_x509write_crt_set_issuer_name(writer, "C=US,O=Android,CN=Adb");
    if (err != 0)
        goto fail;

    mbedtls_x509write_crt_set_subject_key(writer, &key->pk);
    mbedtls_x509write_crt_set_issuer_key(writer, &key->pk);

    err = mbedtls_x509write_crt_set_basic_constraints(writer, 1, -1);
    if (err != 0)
        goto fail;
    err = mbedtls_x509write_crt_set_key_usage(writer,
            MBEDTLS_X509_KU_KEY_CERT_SIGN |
            MBEDTLS_X509_KU_CRL_SIGN |
            MBEDTLS_X509_KU_DIGITAL_SIGNATURE);
    if (err != 0)
        goto fail;
    err = mbedtls_x509write_crt_set_subject_key_identifier(writer);
    if (err != 0)
        goto fail;

    mbedtls_x509write_crt_set_md_alg(writer, MBEDTLS_MD_SHA256);
    return 0;

fail:
    return err;
}

mbedtls_pk_context *adb__key_get_pk(adb_key_t *key)
{
    if (!key)
        return NULL;
    return &key->pk;
}

bool adb__key_create_x509(
        adb_key_t *key,
        adb_ctx_t *ctx,
        mbedtls_x509_crt *crt)
{
    bool ret = false;
    int err = 0;
    size_t cert_len = 0U;
    uint8_t cert_der[4096] = {0};
    static const uint8_t serial[] = {0x01U};
    mbedtls_x509write_cert writer;

    if (!key || !ctx || !crt)
        return false;

    mbedtls_x509write_crt_init(&writer);

    err = mbedtls_x509write_crt_set_serial_raw(
            &writer, serial, sizeof(serial));
    if (err != 0)
        goto cleanup;

    err = adb__key_x509write_cert(key, &writer);
    if (err != 0)
        goto cleanup;

    err = mbedtls_x509write_crt_der(&writer, cert_der, sizeof(cert_der));
    if (err < 0)
        goto cleanup;

    cert_len = (size_t)err;
    err = mbedtls_x509_crt_parse(crt,
            cert_der + sizeof(cert_der) - cert_len, cert_len);
    if (err != 0)
        goto cleanup;

    ret = true;

cleanup:
    if (!ret)
        adb__log_err_mbedtls(err,
                "failed to create X.509 certificate from key");
    mbedtls_x509write_crt_free(&writer);
    return ret;
}

adb_error_t adb__key_sign(
        adb_key_t *key,
        adb_ctx_t *ctx,
        const char *token,
        const size_t token_size,
        uint8_t *out_sig)
{
    size_t sig_len = 0U;
    int err = 0;

    if (!key || !ctx || !token || !out_sig)
        return ADB_ERR_PARAM;
    if (token_size != ADB__TOKEN_SIZE)
        return ADB_ERR_PROTOCOL;

    err = mbedtls_pk_sign(&key->pk, MBEDTLS_MD_SHA1,
            (const uint8_t *)token, token_size,
            out_sig, ADB__RSA_MODULUS_SIZE, &sig_len);
    if (err != 0)
    {
        adb__log_err_mbedtls(err, "failed to sign token");
        return ADB_ERR_CRYPTO;
    }
    if (sig_len != ADB__RSA_MODULUS_SIZE)
        return ADB_ERR_CRYPTO;

    return ADB_ERR_OK;
}
