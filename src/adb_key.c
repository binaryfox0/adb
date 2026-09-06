#include <adb/adb_key.h>
#include "adb_key_priv.h"

#include <stdint.h>  
#include <stdbool.h>  
#include <stddef.h>
#include <string.h>  
  
#include <unistd.h>
#include <pwd.h>

#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/base64.h>
#include <mbedtls/asn1write.h>
#include <mbedtls/pem.h>


#include "adb_alloc_priv.h"
#include "adb_ctx_priv.h"
#include "adb_log_priv.h"

#define ADB__STRINGIFY_IMPL(x) #x
#define ADB__STRINGIFY(x) ADB__STRINGIFY_IMPL(x)

#define ADB__RSA_MODULUS_BITS   2048
#define ADB__RSA_ALGORITHM      "RSA-" ADB__STRINGIFY(ADB__RSA_MODULUS_BITS)
#define ADB__RSA_EXPONENT       65537
#define ADB__RSA_MODULUS_SIZE   (ADB__RSA_MODULUS_BITS / 8)
#define ADB__RSA_MODULUS_WORDS  (ADB__RSA_MODULUS_SIZE / 4)
#define ADB__ANDROID_PUBKEY_SIZE    \
    (3 * sizeof(uint32_t) + 2 * ADB__RSA_MODULUS_SIZE)  
#define ADB__BASE64_SIZE(n) (4 * (((n) + 2) / 3))
#define ADB__RSA2048_DER_MAX 2048
  
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
    char *pubkey;
} adb_key_t;
 
_Static_assert(sizeof(adb__android_pubkey_t) == ADB__ANDROID_PUBKEY_SIZE,
        "adb__android_pubkey must be exactly 524 bytes");

adb_error_t adb_key_generate(
        adb_key_t **key,
        adb_ctx_t *ctx)
{
    adb_error_t ret = ADB_ERR_OK;
    adb_key_t *tmp = NULL;
    int err = 0;

    if(!key || !ctx)
        return ADB_ERR_PARAM;

    ADB__INFO("generating new RSA key pair ("
            ADB__STRINGIFY(ADB__RSA_MODULUS_BITS)" bits)");

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp) 
        return ADB_ERR_NO_MEM;

    mbedtls_pk_init(&tmp->pk);
    err = mbedtls_pk_setup(
            &tmp->pk,
            mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if(err != 0) 
    {
        ret = ADB_ERR_CRYPTO;
        goto cleanup;
    }

    err = mbedtls_rsa_gen_key(
            mbedtls_pk_rsa(tmp->pk),
            mbedtls_ctr_drbg_random,
            &ctx->drbg,
            ADB__RSA_MODULUS_BITS,
            ADB__RSA_EXPONENT);
    if(err != 0) 
    {
        adb__log_err_mbedtls(err, 
                "failed to generate " ADB__RSA_ALGORITHM " key pair");
        ret = ADB_ERR_CRYPTO;
        goto cleanup;
    }

    ADB__INFO(ADB__RSA_ALGORITHM " key pair generated successfully");

    *key = tmp;
    return ADB_ERR_OK;

cleanup:
    adb_key_destroy(tmp);
    return ret;
}

static adb_error_t adb__util_read_file(
        const char *path,
        char **buf_out,
        size_t *size_out)
{
    FILE *file = NULL;
    size_t file_size = 0;
    char *tmp = NULL;
    if(!path || !buf_out || !size_out)
        return ADB_ERR_PARAM;

    file = fopen(path, "r");
    if(!file)
        return ADB_ERR_IO;

    fseek(file, 0, SEEK_END);
    file_size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    tmp = adb__malloc(file_size + 1);
    if(!tmp)
    {
        fclose(file);
        return ADB_ERR_NO_MEM;
    }

    if(fread(tmp, 1, file_size, 
                file) != file_size)
    {
        adb__free(tmp);
        fclose(file);
        return ADB_ERR_IO;
    }
    tmp[file_size] = '\0';

    fclose(file); 

    *buf_out = tmp;
    *size_out = file_size;
    return ADB_ERR_OK;
}


adb_error_t adb_key_load(
        adb_key_t **key,
        adb_ctx_t *ctx,
        const char *path)
{
    adb_error_t ret = ADB_ERR_OK;
    adb_key_t *tmp = NULL;
    char *buffer = NULL;
    size_t file_size = 0;
    int err = 0;

    ADB__INFO("loading key from \"%s\"", path); 

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    ret = adb__util_read_file(path, 
            &buffer, &file_size);
    if(ret != ADB_ERR_OK)
    {
        adb__log_err_errno("failed to load key at \"%s\"", path);
        goto cleanup;
    }
    mbedtls_pk_init(&tmp->pk);
    err = mbedtls_pk_parse_key(
            &tmp->pk, 
            (uint8_t*)buffer, 
            file_size + 1, 
            NULL, 
            0, 
            mbedtls_ctr_drbg_random, 
            &ctx->drbg);
    if(err != 0)
    {
        adb__log_err_mbedtls(err, 
                "failed to process key from \"%s\"", path);
        ret = ADB_ERR_CRYPTO;
        goto cleanup;
    }
    if(!mbedtls_pk_can_do(&tmp->pk, MBEDTLS_PK_RSA) ||
            mbedtls_rsa_get_bitlen(mbedtls_pk_rsa(tmp->pk)) 
                != ADB__RSA_MODULUS_BITS)
    {
        ADB__ERROR("the given key was not " ADB__RSA_ALGORITHM ": \"%s\"", 
                path);
        ret = ADB_ERR_UNSUPPORTED;
        goto cleanup;
    }

    *key = tmp;
    ADB__INFO("loaded key successfully from \"%s\"", path);

cleanup:
    if(ret != ADB_ERR_OK)
        adb_key_destroy(tmp);
    adb__free(buffer);

    return ret;
}

static adb_error_t adb__util_write_file(
        const char *path,
        const uint8_t *buf,
        const size_t size)
{
    FILE *file = NULL;
    if(!path || !buf || size == 0)
        return ADB_ERR_PARAM;

    file = fopen(path, "w");
    if(!file)
        return ADB_ERR_IO;

    if(fwrite(buf, 1, size, file) != size)
    {
        fclose(file);
        return ADB_ERR_IO;
    }
    
    fclose(file);
    return ADB_ERR_OK;
}

static bool adb__encode_android_pubkey(  
        const mbedtls_rsa_context *rsa,  
        adb__android_pubkey_t *out)  
{  
    bool ret = false;
    mbedtls_mpi N = {0};
    mbedtls_mpi r = {0};
    mbedtls_mpi n0 = {0};
    mbedtls_mpi rr = {0};
    int err = 0;
  
    if (!rsa || !out) 
        return false;

    ADB__INFO("encoding android public key from private key");
  
    mbedtls_mpi_init(&N);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&n0);
    mbedtls_mpi_init(&rr);
 
    err = mbedtls_rsa_export(rsa, &N, NULL, NULL, NULL, NULL);
    if(err != 0)
        goto cleanup;
    out->modulus_size_words = ADB__RSA_MODULUS_WORDS;
  
    /*  
     * n0inv = -N^(-1) mod 2^32  
     *  
     * AOSP:  
     *   r32 = 2^32  
     *   n0inv = N mod r32  
     *   n0inv = inverse(n0inv) mod r32  
     *   n0inv = r32 - n0inv  
     */  
 
    if(
            (err = mbedtls_mpi_lset(&r, 0)) != 0 ||  
            (err = mbedtls_mpi_set_bit(&r, 32, 1)) != 0 ||  
            (err = mbedtls_mpi_mod_mpi(&n0, &rsa->private_N, &r)) != 0 ||  
            (err = mbedtls_mpi_inv_mod(&n0, &n0, &r)) != 0 ||  
            (err = mbedtls_mpi_sub_mpi(&n0, &r, &n0)) != 0)
        goto cleanup;
  
    mbedtls_mpi_write_binary_le(
            &n0, 
            (uint8_t*)&out->n0inv, 
            sizeof(out->n0inv));

    err = mbedtls_mpi_write_binary_le(  
        &rsa->private_N,  
        out->modulus,  
        ADB__RSA_MODULUS_SIZE);
    if (err != 0) 
        goto cleanup;
  
    /*  
     * rr = R^2 mod N  
     * R = 2^(RSA_size * 8)  
     *    = 2^2048  
     *  
     * Therefore:  
     *     rr = 2^4096 mod N  
     */  
    err = mbedtls_mpi_lset(&rr, 0);
    if (err != 0) 
        goto cleanup;
  
    err = mbedtls_mpi_set_bit(  
        &rr,  
        ADB__RSA_MODULUS_SIZE * 8,  
        1);
    if (err != 0) 
        goto cleanup;
  
    err = mbedtls_mpi_mod_mpi(&rr, &rr, &rsa->private_N);
    if (err != 0) 
        goto cleanup;
  
    /* rr = rr^2 mod N */  
    if(
            (err = mbedtls_mpi_mul_mpi(&rr, &rr, &rr)) != 0 ||
            (err = mbedtls_mpi_mod_mpi(&rr, &rr, &rsa->private_N)) != 0)
        goto cleanup;
  
    err = mbedtls_mpi_write_binary_le(  
        &rr,  
        out->rr,  
        ADB__RSA_MODULUS_SIZE);
    if (err != 0) 
        goto cleanup;
  
    out->exponent = ADB__RSA_EXPONENT;
    ret = true;

    ADB__INFO("encoded android public key successfully");
  
cleanup:  
    if(err != 0)
    {
        adb__log_err_mbedtls(err, 
                "failed to encode android public key "
                "from private key");
    }

    mbedtls_mpi_free(&rr);
    mbedtls_mpi_free(&n0);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&N);
    return ret;
}  

adb_error_t adb_key_generate_pubkey(
        adb_key_t *key,
        uint8_t *buffer,
        const size_t size,
        size_t *out_size)
{
    adb__android_pubkey_t pubkey = {0};
    size_t encoded_size = ADB__BASE64_SIZE(ADB__ANDROID_PUBKEY_SIZE);
    struct passwd *pw = NULL;
    char hostname[HOST_NAME_MAX + 1] = {0};
    size_t min_size = 0;
    
    if(!key || (!buffer ^ (size == 0)) || !out_size)
        return ADB_ERR_PARAM;

    ADB__INFO("generating public key from private key");

    if(!adb__encode_android_pubkey(mbedtls_pk_rsa(key->pk), &pubkey))
        return ADB_ERR_CRYPTO;

    pw = getpwuid(getuid());
    if(!pw)
    {
        adb__log_err_errno("failed to get current user name");
        return ADB_ERR_GENERIC;
    }
    gethostname(hostname, sizeof(hostname));

    min_size =
        encoded_size + 1 + /* ' ' */
        strlen(pw->pw_name) + 1 + /* '@' */
        strlen(hostname) + 1;/* '\0' */
    if(min_size > size)
        return ADB_ERR_TOO_SMALL;

    *out_size = min_size;
    if(!buffer)
        return ADB_ERR_OK;

    mbedtls_base64_encode(
            buffer, 
            encoded_size, 
            &encoded_size, /* must not be NULL */ 
            (uint8_t*)&pubkey, 
            ADB__ANDROID_PUBKEY_SIZE);
    snprintf(
            (char*)buffer + encoded_size, 
            size - encoded_size, 
            " %s@%s", 
            pw->pw_name, hostname);

uint8_t buf[4096] = {0};
adb__key_write_pkcs8_pem(key, buf, sizeof(buf));
adb__util_write_file("./adbkey.pem", buf, strlen((char*)buf));
    ADB__INFO("generated public key successfully");
    return ADB_ERR_OK;
}

void adb_key_destroy(
        adb_key_t *key)
{
    if(!key)
        return;

    mbedtls_pk_free(&key->pk);
    adb__free(key->pubkey);
    adb__free(key);
}

bool adb__key_write_pkcs8_pem(
        adb_key_t *key,
        uint8_t *out,
        const size_t out_size)
{
    uint8_t der[ADB__RSA2048_DER_MAX] = {0};
    uint8_t *end = NULL;
    size_t pkcs1_len = 0;
    int err = 0;
    uint8_t *p = NULL;
    uint8_t *alg_end = NULL;
    size_t alg_id_len = 0;
    size_t content_len = 0;
    size_t der_len = 0;
    size_t pem_len = 0;

    static const uint8_t rsa_encryption_oid[] = 
    {
        0x06, 0x09,
        0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01
    };

    static const uint8_t null_params[] = {
        0x05, 0x00
    };


    end = der + sizeof(der);
    err = mbedtls_pk_write_key_der(&key->pk, der, sizeof(der));
    if (err < 0) 
        return err;

    pkcs1_len = (size_t)err;
    p = end - pkcs1_len;

    /* privateKey OCTET STRING */
    if(
            (err = mbedtls_asn1_write_len(
                    &p, 
                    der, 
                    pkcs1_len)) < 0 ||
            (err = mbedtls_asn1_write_tag(
                    &p,
             der,
                MBEDTLS_ASN1_OCTET_STRING)) < 0)
        goto fail;

    /* Everything after this point belongs to AlgorithmIdentifier,
     * not to privateKey. */
    alg_end = p;

    /* 
     * AlgorithmIdentifier:
     *     SEQUENCE {
     *         algorithm  OBJECT IDENTIFIER rsaEncryption,
     *         parameters NULL
     *     }
     */
    if(
            (err = mbedtls_asn1_write_raw_buffer(
                    &p, 
                    der, 
                    null_params, 
                    sizeof(null_params))) < 0 ||
            (err = mbedtls_asn1_write_raw_buffer(
                    &p, 
                    der,
                    rsa_encryption_oid, 
                    sizeof(rsa_encryption_oid))) < 0)
        goto fail;

    alg_id_len = (size_t)(alg_end - p);
    if(
            (err = mbedtls_asn1_write_len(
                    &p, 
                    der, 
                    alg_id_len)) < 0 ||
            (err = mbedtls_asn1_write_tag(
                 &p,
             der,
               MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) < 0)
        goto fail;

    /* version INTEGER 0 */
    err = mbedtls_asn1_write_int(&p, der, 0);
    if (err < 0) 
        return err;

    /*
     * PrivateKeyInfo:
     *     SEQUENCE {
     *         version
     *         privateKeyAlgorithm
     *         privateKey
     *     }
     */
    content_len = (size_t)(end - p);
    if(
            (err = mbedtls_asn1_write_len(
                    &p, 
                    der, 
                    content_len)) < 0 ||
            (err = mbedtls_asn1_write_tag(
                 &p,
                 der,
                 MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) < 0)
        goto fail;

    der_len = (size_t)(end - p);
    err = mbedtls_pem_write_buffer(
        "-----BEGIN PRIVATE KEY-----\n",
        "-----END PRIVATE KEY-----\n",
        p,
        der_len,
        out,
        out_size,
        &pem_len);
    if (err != 0) 
        return err;

    return true;

fail:
    adb__log_err_mbedtls(err, "failed to write pkcs#8 pem from key");
    return false;
}
