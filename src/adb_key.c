#include <adb/adb_key.h>
#include "adb_key_priv.h"

#include <stdint.h>  
#include <stdbool.h>  
#include <string.h>  
  
#include <unistd.h>
#include <pwd.h>

#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/base64.h>

#include "adb_alloc_priv.h"
#include "adb_ctx_priv.h"
#include "adb_log_priv.h"

#define ADB__RSA_MODULUS_BITS   2048
#define ADB__RSA_EXPONENT       65537
#define ADB__RSA_MODULUS_SIZE   (ADB__RSA_MODULUS_BITS / 8)
#define ADB__RSA_MODULUS_WORDS  (ADB__RSA_MODULUS_SIZE / 4)
#define ADB__PUBKEY_ENCODED_SIZE    \
    (3 * sizeof(uint32_t) + 2 * ADB__RSA_MODULUS_SIZE)  
  
  
typedef struct {  
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
 
_Static_assert(sizeof(adb__android_pubkey_t) == ADB__PUBKEY_ENCODED_SIZE,
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

    ADB__INFO("generating RSA key pair (2048 bits)");

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
        adb__log_err_mbedtls(err, "failed to generate RSA-2048 key pair");
        ret = ADB_ERR_CRYPTO;
        goto cleanup;
    }

    ADB__INFO("RSA-2048 key pair generated successfully");

    *key = tmp;
    return ADB_ERR_OK;

cleanup:
    adb_key_destroy(tmp);
    return ret;
}

adb_error_t adb_key_load(
        adb_key_t **key,
        adb_ctx_t *ctx,
        const char *path)
{
    adb_error_t ret = ADB_ERR_OK;
    adb_key_t *tmp = NULL;
    FILE *file = NULL;
    size_t file_size = 0;
    char *buffer = NULL;
    int err = 0;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    file = fopen(path, "r");
    if(!file)
    {
        adb__log_err_errno("failed to load private key at \"%s\"", path);
        ret = ADB_ERR_IO;
        goto cleanup;
    }

    fseek(file, 0, SEEK_END);
    file_size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = adb__malloc(file_size + 1);
    if(!buffer)
        { ret = ADB_ERR_NO_MEM; goto cleanup; }
    
    if(fread(buffer, 1, file_size, file) != file_size)
        { ret = ADB_ERR_IO; goto cleanup; }
    buffer[file_size] = '\0';
    fclose(file);
    
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
                "failed to load private key from \"%s\"", path);
        ret = ADB_ERR_CRYPTO;
        goto cleanup;
    }
    if(!mbedtls_pk_can_do(&tmp->pk, MBEDTLS_PK_RSA) ||
            mbedtls_rsa_get_bitlen(mbedtls_pk_rsa(tmp->pk)) != ADB__RSA_MODULUS_BITS)
    {
        ADB__ERROR("given private key was not RSA-2048: \"%s\"", path);
        ret = ADB_ERR_UNSUPPORTED;
        goto cleanup;
    }

    *key = tmp;

cleanup:
    if(ret != ADB_ERR_OK)
        adb_key_destroy(tmp);
    adb_free(buffer);
    fclose(file);

    return ret;
}

static bool adb__encode_android_pubkey(  
        const mbedtls_rsa_context *rsa,  
        adb__android_pubkey_t *out)  
{  
    int ret;
    mbedtls_mpi r;  
    mbedtls_mpi n0;  
    mbedtls_mpi rr;  
  
    if (!rsa || !out) 
        return false;    
  
    mbedtls_mpi_init(&r);  
    mbedtls_mpi_init(&n0);  
    mbedtls_mpi_init(&rr);  
  
    /*  
     * modulus_size_words = 2048 / 32 = 64  
     */  
    out->modulus_size_words = ADB__RSA_MODULUS_WORDS;  
  
    /*  
     * n0inv = -N^(-1) mod 2^32  
     *  
     * AOSP:  
     *  
     *   r32 = 2^32  
     *   n0inv = N mod r32  
     *   n0inv = inverse(n0inv) mod r32  
     *   n0inv = r32 - n0inv  
     */  
  
    ret = mbedtls_mpi_lset(&r, 0);  
    if (ret != 0) 
        goto fail;    
  
    ret = mbedtls_mpi_set_bit(&r, 32, 1);  
    if (ret != 0) 
        goto fail;    
  
    ret = mbedtls_mpi_mod_mpi(&n0, &rsa->private_N, &r);  
    if (ret != 0) 
        goto fail;    
  
    ret = mbedtls_mpi_inv_mod(&n0, &n0, &r);  
    if (ret != 0) 
        goto fail;    
  
    ret = mbedtls_mpi_sub_mpi(&n0, &r, &n0);  
    if (ret != 0) 
        goto fail;    
  
    mbedtls_mpi_write_binary_le(
            &n0, 
            (uint8_t*)&out->n0inv, 
            sizeof(out->n0inv));

    ret = mbedtls_mpi_write_binary_le(  
        &rsa->private_N,  
        out->modulus,  
        ADB__RSA_MODULUS_SIZE);  
    if (ret != 0) 
        goto fail;    
  
    /*  
     * rr = R^2 mod N  
     *  
     * R = 2^(RSA_size * 8)  
     *    = 2^2048  
     *  
     * Therefore:  
     *  
     *     rr = 2^4096 mod N  
     */  
    ret = mbedtls_mpi_lset(&rr, 0);  
    if (ret != 0) 
        goto fail;    
  
    ret = mbedtls_mpi_set_bit(  
        &rr,  
        ADB__RSA_MODULUS_SIZE * 8,  
        1);  
    if (ret != 0) 
        goto fail;    
  
    ret = mbedtls_mpi_mod_mpi(&rr, &rr, &rsa->private_N);  
    if (ret != 0) 
        goto fail;    
  
    /*  
     * rr = rr^2 mod N  
     */  
    ret = mbedtls_mpi_mul_mpi(&rr, &rr, &rr);  
    if (ret != 0) 
        goto fail;    
  
    ret = mbedtls_mpi_mod_mpi(&rr, &rr, &rsa->private_N);  
    if (ret != 0) 
        goto fail;    
  
    ret = mbedtls_mpi_write_binary_le(  
        &rr,  
        out->rr,  
        ADB__RSA_MODULUS_SIZE);  
    if (ret != 0) 
        goto fail;    
  
    out->exponent = ADB__RSA_EXPONENT;
  
    mbedtls_mpi_free(&rr);  
    mbedtls_mpi_free(&n0);  
    mbedtls_mpi_free(&r);  
  
    return true;  
  
fail:  
    mbedtls_mpi_free(&rr);  
    mbedtls_mpi_free(&n0);  
    mbedtls_mpi_free(&r);  
  
    return false;  
}  

adb_error_t adb_key_generate_pubkey(
        adb_key_t *key,
        char **out)
{
    adb__android_pubkey_t pubkey = {0};
    size_t encoded_size = 0;
    char *tmp = NULL;
    struct passwd *pw = NULL;
    char hostname[HOST_NAME_MAX + 1] = {0};
    size_t size = 0;
    
    if(!key || !out)
        return ADB_ERR_PARAM;

    if(!adb__encode_android_pubkey(mbedtls_pk_rsa(key->pk), &pubkey))
        return ADB_ERR_CRYPTO;

    mbedtls_base64_encode(
            NULL, 
            0, 
            &encoded_size, 
            (uint8_t*)&pubkey, 
            ADB__PUBKEY_ENCODED_SIZE);
    pw = getpwuid(getuid());
    if(!pw)
    {
        adb__log_err_errno("failed to get current user name");
        return ADB_ERR_GENERIC;
    }
    gethostname(hostname, sizeof(hostname));

    size =
        encoded_size + 1 + /* ' ' */
        strlen(pw->pw_name) + 1 + /* '@' */
        strlen(hostname) + 1, /* '\0' */

    tmp = adb__malloc(size);
    if(!tmp)
        return ADB_ERR_NO_MEM;

    mbedtls_base64_encode(
            (uint8_t*)tmp, 
            encoded_size, 
            &encoded_size, /* must not be NULL */ 
            (uint8_t*)&pubkey, 
            ADB__PUBKEY_ENCODED_SIZE);
    snprintf(
            tmp + encoded_size, 
            size - encoded_size, 
            " %s@%s", 
            pw->pw_name, hostname);
    
    *out = tmp; 
    return ADB_ERR_OK;
}

void adb_key_destroy(
        adb_key_t *key)
{
    if(!key)
        return;

    mbedtls_pk_free(&key->pk);
    adb_free(key->pubkey);
    adb_free(key);
}

