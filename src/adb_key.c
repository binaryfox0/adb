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
#define ADB__ANDROID_PUBKEY_SIZE    \
    (3 * sizeof(uint32_t) + 2 * ADB__RSA_MODULUS_SIZE)  
#define ADB__BASE64_SIZE(n) (4 * (((n) + 2) / 3))
  
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
    adb__free(buffer);
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
 
    if(
            (ret = mbedtls_mpi_lset(&r, 0)) != 0 ||  
            (ret = mbedtls_mpi_set_bit(&r, 32, 1)) != 0 ||  
            (ret = mbedtls_mpi_mod_mpi(&n0, &rsa->private_N, &r)) != 0 ||  
            (ret = mbedtls_mpi_inv_mod(&n0, &n0, &r)) != 0 ||  
            (ret = mbedtls_mpi_sub_mpi(&n0, &r, &n0)) != 0)
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
  
    /* rr = rr^2 mod N */  
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
        char *buffer,
        const size_t size,
        size_t *out_size)
{
    adb__android_pubkey_t pubkey = {0};
    size_t encoded_size = ADB__BASE64_SIZE(ADB__ANDROID_PUBKEY_SIZE);
    char *tmp = NULL;
    struct passwd *pw = NULL;
    char hostname[HOST_NAME_MAX + 1] = {0};
    size_t min_size = 0;
    
    if(!key || (!buffer ^ (size == 0)) || !out_size)
        return ADB_ERR_PARAM;

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
        strlen(hostname) + 1; /* '\0' */
    if(min_size > size)
        return ADB_ERR_TOO_SMALL;

    *out_size = min_size;
    if(!buffer)
        return ADB_ERR_OK;

    mbedtls_base64_encode(
            (uint8_t*)buffer, 
            encoded_size, 
            &encoded_size, /* must not be NULL */ 
            (uint8_t*)&pubkey, 
            ADB__ANDROID_PUBKEY_SIZE);
    snprintf(
            tmp + encoded_size, 
            size - encoded_size, 
            " %s@%s", 
            pw->pw_name, hostname);
    
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

#include <stddef.h>
#include <stdint.h>

#include "mbedtls/asn1write.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"

/*
 * Write an RSA-2048 private key as unencrypted PKCS#8 PrivateKeyInfo DER.
 *
 * The supplied buffer is used as scratch/output storage.
 * The DER is written at the END of the buffer, following the same
 * convention as mbedtls_pk_write_key_der().
 *
 * On success:
 *   - *out points to the beginning of the PKCS#8 DER
 *   - *out_len is its length
 *
 * Expected output:
 *
 *   PrivateKeyInfo ::= SEQUENCE {
 *       version                   INTEGER 0,
 *       privateKeyAlgorithm      AlgorithmIdentifier {
 *           rsaEncryption,
 *           NULL
 *       },
 *       privateKey                OCTET STRING { RSAPrivateKey DER }
 *   }
 *
 * Returns 0 on success, otherwise a negative Mbed TLS error code.
 */
int write_rsa2048_pkcs8_der( adb_key_t *key,
                             unsigned char *buf,
                             size_t buf_size,
                             unsigned char **out,
                             size_t *out_len )
{
    unsigned char *p;
    unsigned char *start = buf;
    int ret;
    int len;

    /*
     * rsaEncryption OID:
     *
     * 1.2.840.113549.1.1.1
     *
     * DER value:
     *   06 09 2A 86 48 86 F7 0D 01 01 01
     *
     * mbedtls_asn1_write_algorithm_identifier() expects the
     * raw OID contents, not the DER 06 09 prefix.
     */
    static const unsigned char rsa_encryption_oid[] = {
        0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01
    };

    if (!key || buf == NULL || out == NULL || out_len == NULL) {
        return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
    }

    *out = NULL;
    *out_len = 0;

    p = buf + buf_size;

    /*
     * Step 1:
     *
     * Write the existing RSA private-key DER.
     *
     * This gives us:
     *
     *   RSAPrivateKey ::= SEQUENCE {
     *       version           INTEGER,
     *       modulus           INTEGER,
     *       publicExponent    INTEGER,
     *       privateExponent   INTEGER,
     *       prime1            INTEGER,
     *       prime2            INTEGER,
     *       exponent1         INTEGER,
     *       exponent2         INTEGER,
     *       coefficient       INTEGER
     *   }
     *
     * and p points to its beginning.
     */
    ret = mbedtls_pk_write_key_der(&key->pk, buf, buf_size);
    if (ret < 0) {
        return ret;
    }

    len = ret;
    p = buf + buf_size - len;

    /*
     * Step 2:
     *
     * Wrap the PKCS#1 DER in:
     *
     *   OCTET STRING
     *
     * We do this manually rather than copying the existing DER,
     * because the ASN.1 writer works backwards and the PKCS#1
     * bytes are already exactly where we want them.
     */
    ret = mbedtls_asn1_write_len(&p, start, (size_t) len);
    if (ret < 0) {
        return ret;
    }

    ret = mbedtls_asn1_write_tag(&p, start, MBEDTLS_ASN1_OCTET_STRING);
    if (ret < 0) {
        return ret;
    }

    /*
     * Step 3:
     *
     * AlgorithmIdentifier:
     *
     *   SEQUENCE {
     *       algorithm  OBJECT IDENTIFIER rsaEncryption
     *       parameters NULL
     *   }
     *
     * par_len = 0 means "write a NULL parameter".
     */
    ret = mbedtls_asn1_write_algorithm_identifier(
        &p,
        start,
        (const char *) rsa_encryption_oid,
        sizeof(rsa_encryption_oid),
        0
    );
    if (ret < 0) {
        return ret;
    }

    /*
     * Step 4:
     *
     * PKCS#8 version = 0.
     */
    ret = mbedtls_asn1_write_int(&p, start, 0);
    if (ret < 0) {
        return ret;
    }

    /*
     * Step 5:
     *
     * Wrap everything in the outer PrivateKeyInfo SEQUENCE.
     */
    len = (int) (buf + buf_size - p);

    ret = mbedtls_asn1_write_len(&p, start, (size_t) len);
    if (ret < 0) {
        return ret;
    }

    ret = mbedtls_asn1_write_tag(
        &p,
        start,
        MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE
    );
    if (ret < 0) {
        return ret;
    }

    *out = p;
    *out_len = (size_t) (buf + buf_size - p);

    return 0;
}
