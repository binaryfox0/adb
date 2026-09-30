#include "adb_tls.h"

#include <string.h>

#include <mbedtls/ssl.h>

#include "adb_error_priv.h"
#include "adb_log_priv.h"
#include "adb_transport.h"
#include "adb_ctx_priv.h"
#include "adb_key_priv.h"
#include "adb_alloc_priv.h"
#include "adb_queue.h"
#include "adb_utils.h"

typedef struct adb__tls
{
    adb__queue_t queue;
    adb__transport_t *transport;

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt crt;

    adb_error_t bio_error;
} adb__tls_t;

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
    tls->bio_error = err;
    if(err != ADB_ERR_OK)
    {
        if(err == ADB_ERR_DISCONNECTED)
            return MBEDTLS_ERR_SSL_CONN_EOF;
        else
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

    tls->bio_error = err;
    if(err != ADB_ERR_OK)
    {
        if(err == ADB_ERR_DISCONNECTED)
            return MBEDTLS_ERR_SSL_CONN_EOF;
        else
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    return (int)size;
}

adb_error_t adb__tls_create(
        adb__tls_t **out_tls,
        adb_ctx_t *ctx,
        adb_key_t *key,
        adb__transport_t *transport)
{
    adb__tls_t *tmp = NULL;
    int err = 0;
    if(
            !ctx || !key || !transport || 
            !transport->read || 
            !transport->write)
        return ADB_ERR_PARAM;
    
    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;
    
    mbedtls_ssl_init(&tmp->ssl);
    mbedtls_ssl_config_init(&tmp->conf);
    mbedtls_x509_crt_init(&tmp->crt);

    err = mbedtls_ssl_config_defaults(
            &tmp->conf,
            MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM,
            MBEDTLS_SSL_PRESET_DEFAULT);

    if(err != 0)
    {
        adb__log_err_mbedtls(err, "failed to configure TLS");
        goto fail;
    }

    mbedtls_ssl_conf_rng(
            &tmp->conf,
            mbedtls_ctr_drbg_random,
            &ctx->drbg);
    
    mbedtls_ssl_conf_min_tls_version(
            &tmp->conf,
            MBEDTLS_SSL_VERSION_TLS1_3);

    mbedtls_ssl_conf_max_tls_version(
            &tmp->conf,
            MBEDTLS_SSL_VERSION_TLS1_3);

    mbedtls_ssl_conf_authmode(
            &tmp->conf,
            MBEDTLS_SSL_VERIFY_NONE);

    if(!adb__key_create_x509(key, ctx, &tmp->crt))
        return ADB_ERR_CRYPTO;
    err = mbedtls_ssl_conf_own_cert(
            &tmp->conf,
            &tmp->crt,
            adb__key_get_pk(key));
    if(err != 0)
    {
        adb__log_err_mbedtls(err, "failed to set X.509 certificate");
        goto fail;
    }
            
    err = mbedtls_ssl_setup(
            &tmp->ssl,
            &tmp->conf);
    if(err != 0)
    {
        adb__log_err_mbedtls(err, "failed to setup TLS");
        goto fail;
    }

    mbedtls_ssl_set_bio(
            &tmp->ssl,
            tmp,
            adb__tls_bio_send,
            adb__tls_bio_recv,
            NULL);

    tmp->transport = transport;
    tmp->bio_error = ADB_ERR_OK;
    *out_tls = tmp;
    return ADB_ERR_OK;

fail:
    mbedtls_ssl_free(&tmp->ssl);
    mbedtls_ssl_config_free(&tmp->conf);
    mbedtls_x509_crt_free(&tmp->crt);
    adb__free(tmp);
    return ADB_ERR_CRYPTO;
}

adb_error_t adb__tls_handshake(
        adb__tls_t *tls)
{
    int err = 0;
    if(!tls)
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
    static const char label[] = "adb-label";
    int err = 0;
    if(!tls || !out)
        return ADB_ERR_PARAM;

    err = mbedtls_ssl_export_keying_material(
            &tls->ssl,
            out,
            ADB__TLS_EXPORTED_KEY_LENGTH,
            label,
            sizeof(label),
            NULL, 0, 0);

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
    if(!tls)
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

        return ADB_ERR_IO;
    }

    return ADB_ERR_OK;
}

adb_error_t adb__tls_read_timeout(
        adb__tls_t *tls,
        void *buf,
        const size_t size,
        const uint32_t timeout_ms)
{
    adb_error_t res = ADB_ERR_OK;
    adb__queue_t *queue = NULL;
    uint64_t deadline = 0;
    uint64_t now = 0;
    uint64_t remaining = 0;
    int ret = 0;
    uint8_t temp[4096];

    if(!tls || !buf || size == 0)
        return ADB_ERR_PARAM;

    queue = &tls->queue;
    deadline = adb__util_monotonic_ms() + timeout_ms;
    for(;;)
    {
        if(queue->size >= size)
            return adb__queue_pop(queue, buf, size);

        now = adb__util_monotonic_ms();
        if(now >= deadline)
            return ADB_ERR_TIMEOUT;

        remaining = deadline - now;
        if(remaining > UINT32_MAX)
            remaining = UINT32_MAX;

        mbedtls_ssl_conf_read_timeout(&tls->conf, (uint32_t)remaining);
        ret = mbedtls_ssl_read(
                &tls->ssl,
                temp,
                sizeof(temp));
        mbedtls_ssl_conf_read_timeout(&tls->conf, 0);

        if(ret > 0)
        {
            res = adb__queue_push(queue, temp, (size_t)ret);
            if(res != ADB_ERR_OK)
                return res;
            continue;
        }

        if(ret == 0 ||
           ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
           ret == MBEDTLS_ERR_SSL_CONN_EOF ||
           ret == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)
            return ADB_ERR_DISCONNECTED;

        if(ret == MBEDTLS_ERR_SSL_TIMEOUT)
            return ADB_ERR_TIMEOUT;

        return ADB_ERR_IO;
    }
}

adb_error_t adb__tls_write(
        adb__tls_t *tls,
        const void *buf,
        size_t size)
{
    size_t offset = 0;
    int ret = 0;

    if(!tls)
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
        {
            adb__log_err_adb(tls->bio_error, "TLS write failed");
            return tls->bio_error;
        }
        
        adb__log_err_mbedtls(ret, "TLS write failed");

        if(ret == 0 ||
           ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
           ret == MBEDTLS_ERR_SSL_CONN_EOF ||
           ret == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)
            return ADB_ERR_DISCONNECTED;

        return ADB_ERR_IO;
    }

    return ADB_ERR_OK;
}

void adb__tls_destroy(
        adb__tls_t *tls)
{
    if(!tls)
        return;

    adb__queue_destroy(&tls->queue);
    mbedtls_ssl_free(&tls->ssl);
    mbedtls_ssl_config_free(&tls->conf);
    mbedtls_x509_crt_free(&tls->crt);

    adb__free(tls);
}
