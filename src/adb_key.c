#include "adb/adb_key.h"
#include "adb_alloc_priv.h"

#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include "adb_ctx_priv.h"

#define ADB__KEY_RSA_BITS 2048
#define ADB__KEY_RSA_EXP 65537

typedef struct adb_key
{
    mbedtls_pk_context pk;
} adb_key_t;

adb_error_t adb_key_generate(
        adb_ctx_t *ctx,
        adb_key_t **key)
{
    adb_error_t ret = ADB_ERR_OK;
    adb_key_t *tmp = NULL;
    int err = 0;

    if(!key)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp) {
        ret = ADB_ERR_NO_MEM;
        goto cleanup;
    }

    mbedtls_pk_init(&tmp->pk);
    err = mbedtls_ctr_drbg_seed(
            &drbg,
            mbedtls_entropy_func,
            &entropy,
            NULL,
            0);
    if(err != 0) 
    {
        ret = ADB_ERR_CRYPTO;
        goto cleanup;
    }

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
            &drbg,
            ADB__KEY_RSA_BITS,
            ADB__KEY_RSA_EXP);
    if(err != 0) {
        ret = ADB_ERR_CRYPTO;
        goto cleanup;
    }

    *key = tmp;

cleanup:
    if(ret != ADB_ERR_OK && tmp) 
        adb_key_destroy(tmp);

    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);

    return ret;
}

void adb_key_destroy(
        adb_key_t *key)
{
    if(!key)
        return;

    mbedtls_pk_free(&key->pk);
    adb__free(key);
}
