#include "adb_tls.h"

#include <string.h>

#include <mbedtls/ssl.h>

#include "adb_error_priv.h"
#include "adb_log_priv.h"
#include "adb_transport.h"


static int adb__tls_bio_send(
        void *userdata,
        const unsigned char *buf,
        size_t size)
{
    adb__tls_t *tls = userdata;

    adb_error_t err = ADB_ERR_OK;

    if(!tls || !tls->transport->write)
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;

    err = tls->transport->write(
            tls->transport->userdata,
            buf, size);

    if(err != ADB_ERR_OK)
    {
        tls->bio_error = err;
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    return (int)size;
}


static int adb__tls_bio_recv(
        void *userdata,
        unsigned char *buf,
        size_t size)
{
    adb__tls_t *tls = userdata;

    adb_error_t err = ADB_ERR_OK;

    if(!tls || !tls->transport->read)
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;

    err = tls->transport->read(
            tls->transport->userdata,
            buf,
            size);

    if(err != ADB_ERR_OK)
    {
        tls->bio_error = err;
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    return (int)size;
}


adb_error_t adb__tls_init(
        adb__tls_t *tls,
        adb__transport_t *transport)
{
    int err = 0;

    if(!tls || !transport ||
       !transport->read ||
       !transport->write)
        return ADB_ERR_PARAM;

    memset(tls, 0, sizeof(*tls));

    mbedtls_ssl_init(&tls->ssl);
    mbedtls_ssl_config_init(&tls->conf);
    mbedtls_ctr_drbg_init(&tls->drbg);
    mbedtls_entropy_init(&tls->entropy);

    err = mbedtls_ctr_drbg_seed(
            &tls->drbg,
            mbedtls_entropy_func,
            &tls->entropy,
            NULL,
            0);

    if(err != 0)
    {
        adb__log_err_mbedtls(err, "failed to seed TLS RNG");
        goto fail;
    }

    err = mbedtls_ssl_config_defaults(
            &tls->conf,
            MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM,
            MBEDTLS_SSL_PRESET_DEFAULT);

    if(err != 0)
    {
        adb__log_err_mbedtls(err, "failed to configure TLS");
        goto fail;
    }

    mbedtls_ssl_conf_authmode(
            &tls->conf,
            MBEDTLS_SSL_VERIFY_NONE);

    mbedtls_ssl_conf_rng(
            &tls->conf,
            mbedtls_ctr_drbg_random,
            &tls->drbg);

    err = mbedtls_ssl_setup(
            &tls->ssl,
            &tls->conf);

    if(err != 0)
    {
        adb__log_err_mbedtls(err, "failed to setup TLS");
        goto fail;
    }

    tls->transport = transport;
    tls->bio_error = ADB_ERR_OK;

    mbedtls_ssl_set_bio(
            &tls->ssl,
            tls,
            adb__tls_bio_send,
            adb__tls_bio_recv,
            NULL);

    tls->initialized = true;
    return ADB_ERR_OK;

fail:
    mbedtls_ssl_free(&tls->ssl);
    mbedtls_ssl_config_free(&tls->conf);
    mbedtls_ctr_drbg_free(&tls->drbg);
    mbedtls_entropy_free(&tls->entropy);

    memset(tls, 0, sizeof(*tls));

    return ADB_ERR_CRYPTO;
}

adb_error_t adb__tls_handshake(
        adb__tls_t *tls)
{
    int err = 0;
    if(!tls || !tls->initialized)
        return ADB_ERR_PARAM;
    
    err = mbedtls_ssl_handshake(&tls->ssl);
    if(err != 0)
    {
        adb__log_err_mbedtls(err, "failed to perform TLS handshake");
        if(tls->bio_error != ADB_ERR_OK)
        {
            ADB__INFO("send/recv reason: %s", 
                    adb_strerror(tls->bio_error));
            return tls->bio_error;
        }

        return ADB_ERR_NETWORK;
    }

    return ADB_ERR_OK;
}

adb_error_t adb__tls_export_keying_material(
        adb__tls_t *tls,
        uint8_t *out)
{
    int err = 0;
    if(!tls || !tls->initialized || !out)
        return ADB_ERR_PARAM;

    err = mbedtls_ssl_export_keying_material(
            &tls->ssl,
            out,
            ADB__TLS_EXPORTED_KEY_SIZE,
            "adb-label",
            sizeof("adb-label"),
            0, 0, 0);

    if(err != 0)
    {
        adb__log_err_mbedtls(err, "failed to get TLS keying material");
        return ADB_ERR_CRYPTO;
    }

    return ADB_ERR_OK;
}

adb_error_t adb__tls_read(
        adb__tls_t *tls,
        void *buf,
        size_t size)
{
    size_t offset = 0;
    int ret = 0;

    if(!tls || !tls->initialized)
        return ADB_ERR_PARAM;

    if(!buf && size)
        return ADB_ERR_PARAM;

    while(offset < size)
    {
        tls->bio_error = ADB_ERR_OK;

        ret = mbedtls_ssl_read(
                &tls->ssl,
                (unsigned char *)buf + offset,
                size - offset);

        if(ret > 0)
        {
            offset += (size_t)ret;
            continue;
        }

        if(tls->bio_error != ADB_ERR_OK)
            return tls->bio_error;

        adb__log_err_mbedtls(ret, "TLS read failed");

        if(ret == 0 ||
           ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
           ret == MBEDTLS_ERR_SSL_CONN_EOF ||
           ret == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)
            return ADB_ERR_DISCONNECTED;

        return ADB_ERR_CRYPTO;
    }

    return ADB_ERR_OK;
}


adb_error_t adb__tls_write(
        adb__tls_t *tls,
        const void *buf,
        size_t size)
{
    size_t offset = 0;
    int ret = 0;

    if(!tls || !tls->initialized)
        return ADB_ERR_PARAM;

    if(!buf && size)
        return ADB_ERR_PARAM;

    while(offset < size)
    {
        tls->bio_error = ADB_ERR_OK;

        ret = mbedtls_ssl_write(
                &tls->ssl,
                (const unsigned char *)buf + offset,
                size - offset);

        if(ret > 0)
        {
            offset += (size_t)ret;
            continue;
        }

        if(tls->bio_error != ADB_ERR_OK)
            return tls->bio_error;
        
        adb__log_err_mbedtls(ret, "TLS write failed");

        if(ret == 0 ||
           ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
           ret == MBEDTLS_ERR_SSL_CONN_EOF ||
           ret == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)
            return ADB_ERR_DISCONNECTED;

        return ADB_ERR_CRYPTO;
    }

    return ADB_ERR_OK;
}

void adb__tls_destroy(
        adb__tls_t *tls)
{
    if(!tls || !tls->initialized)
        return;

    mbedtls_ssl_free(&tls->ssl);
    mbedtls_ssl_config_free(&tls->conf);
    mbedtls_ctr_drbg_free(&tls->drbg);
    mbedtls_entropy_free(&tls->entropy);

    memset(tls, 0, sizeof(*tls));
}
