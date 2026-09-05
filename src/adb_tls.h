#ifndef ADB_TLS_H
#define ADB_TLS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>

#include <adb/adb_error.h>

typedef struct adb__transport adb__transport_t;
typedef struct
{
    adb__transport_t *transport;

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_context entropy;

    adb_error_t bio_error;

    bool initialized;
} adb__tls_t;

adb_error_t adb__tls_init(
        adb__tls_t *tls,
        adb__transport_t *transport);

adb_error_t adb__tls_handshake(
        adb__tls_t *tls);

adb_error_t adb__tls_read(
        adb__tls_t *tls,
        void *buf,
        size_t size);

adb_error_t adb__tls_write(
        adb__tls_t *tls,
        const void *buf,
        size_t size);

void adb__tls_destroy(
        adb__tls_t *tls);

#endif
