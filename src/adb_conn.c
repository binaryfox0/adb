#include <adb/adb_conn.h>

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/errno.h>

#include <libusb.h>
#include <mbedtls/error.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#include "adb/adb_error.h"
#include "adb_log_priv.h"
#include "adb_ctx_priv.h"
#include "adb_alloc_priv.h"
#include "adb_error_priv.h"
#include "adb_query_priv.h"
#include "adb_packet.h"

typedef enum
{
    ADB__CONN_TYPE_WIRED,
    ADB__CONN_TYPE_WIRELESS,
    ADB__CONN_TYPE_CUSTOM
} adb__conn_type_t;

typedef struct
{
    libusb_device_handle *handle;
    uint8_t read_ep;
    uint8_t write_ep;
} adb__conn_usb_t;

typedef struct
{
    adb_error_t last_error;

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_context entropy;
} adb__conn_tls_t;

typedef struct
{
    adb_read_callback_t read;
    adb_write_callback_t write;
    void *userdata;
    adb_error_t last_error;
} adb__conn_custom_t;

typedef struct adb_conn
{
    adb_ctx_t *ctx;
    adb__conn_type_t type;
    adb_conn_profile_t profile;

    union
    {
        adb__conn_usb_t usb;
        adb__conn_tls_t tls;
        adb__conn_custom_t custom;
    };


    void *libdata;
    adb_error_t (*read)(
            void *userdata, 
            void *buf, 
            const size_t size);
    adb_error_t (*write)(
            void *userdata, 
            const void *buf, 
            const size_t size);
} adb_conn_t;


static adb_error_t adb__read_libusb(
        void *userdata,
        void *buf,
        const size_t size)
{
    adb__conn_usb_t *usb = userdata;
    int transferred = 0;
    int ret = 0;
    
    if(!usb || (!buf && size))
        return ADB_ERR_PARAM;

    ret = libusb_bulk_transfer(
            usb->handle,
            usb->read_ep,
            buf,
            (int)size,
            &transferred,
            0);

    if(ret != LIBUSB_SUCCESS)
        return adb__error_from_libusb(ret);

    if(transferred != (int)size)
        return ADB_ERR_IO;

    return ADB_ERR_OK;
}

static adb_error_t adb__write_libusb(
        void *userdata,
        const void *buf,
        const size_t size)
{
    adb__conn_usb_t *usb = userdata;
    int transferred = 0;
    int ret = 0;

    ret = libusb_bulk_transfer(
            usb->handle,
            usb->write_ep,
            (uint8_t*)(uintptr_t)buf,
            (int)size,
            &transferred,
            0);

    if(ret != LIBUSB_SUCCESS)
        return adb__error_from_libusb(ret);

    if(transferred != (int)size)
        return ADB_ERR_IO;

    return ADB_ERR_OK;
}

adb_error_t adb_conn_create_wired(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const adb_wired_info_t *info)
{
    adb_conn_t *tmp = NULL;
    int res = 0;
    libusb_device_handle *handle = NULL;
    if(!conn || !ctx || !info)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;
                                 
    ADB__INFO("creating connection for USB device %04X:%04X %s %s",
            info->vendor_id, info->product_id,
            info->manufacturer, info->product);
    
    res = libusb_open(info->device, &handle);
    if(res != 0)
    {
        ADB__ERROR("failed to open USB device %04X:%04X %s %s",
                info->vendor_id, info->product_id,
                info->manufacturer, info->product);
        ADB__INFO("reason: %s (%s)",
                    libusb_error_name(res),
                    libusb_strerror(res));
        adb__free(tmp);
        goto fail;
    }

    res = libusb_claim_interface(handle, 
            info->itf_idx);
    if(res != 0)
    {
        ADB__ERROR("failed to claim USB device interface %u: %04X:%04X %s %s",
                info->itf_idx,
                info->vendor_id, info->product_id,
                info->manufacturer, info->product);
        ADB__INFO("reason: %s (%s)",
                    libusb_error_name(res),
                    libusb_strerror(res));
        goto fail;
    }

    tmp->ctx = ctx;
    tmp->type = ADB__CONN_TYPE_WIRED;
    tmp->profile = ADB_CONN_PROFILE_WIRED;

    tmp->usb.handle = handle;
    tmp->usb.read_ep = info->read_ep;
    tmp->usb.write_ep = info->write_ep;

    tmp->libdata = &tmp->usb;
    tmp->read = adb__read_libusb;
    tmp->write = adb__write_libusb;
    *conn = tmp;

    ADB__INFO("created connection successfully for USB device "
            "%04X:%04X %s %s (interface %u)",
            info->vendor_id, info->product_id,
            info->manufacturer, info->product,
            info->itf_idx);
    return ADB_ERR_OK;

fail:
    if(handle)
        libusb_close(handle);
    adb__free(tmp);
    return ADB_ERR_USB;
}


static int adb__read_tcp(
        void *userdata,
        unsigned char *buf,
        size_t size)
{
    ssize_t res = recv(
            (int)(uintptr_t)userdata,
            buf, size,
            0);

    if(res < 0)
    {
        tls->last_error = adb__error_from_errno(errno);
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    if(res == 0)
        return 0;

    return (int)res;
}

static int adb__write_tcp(
        void *userdata,
        const unsigned char *buf,
        size_t size)
{
    adb__conn_tls_t *tls = userdata;
    ssize_t res = send(
            tls->sock, 
            buf, size, 0);
    if(res < 0)
    {
        tls->last_error = adb__error_from_errno(errno);
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    else if(res == 0)
        return MBEDTLS_ERR_SSL_CONN_EOF;
    return (int)res;
}

static adb_error_t adb__read_tls(
        void *userdata,
        void *buf,
        const size_t size)
{
    adb__conn_tls_t *tls = userdata;
    size_t offset = 0;
    int ret = 0;

    if(!tls || (!buf && size))
        return ADB_ERR_PARAM;

    while(offset < size)
    {
        ret = mbedtls_ssl_read(
                &tls->ssl,
                (uint8_t*)buf + offset,
                size - offset);

        if(ret > 0)
        {
            offset += (size_t)ret;
            continue;
        }

        if(ret == 0 ||
           ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
           ret == MBEDTLS_ERR_SSL_CONN_EOF)
            return ADB_ERR_DISCONNECTED;

        if(ret == MBEDTLS_ERR_SSL_INTERNAL_ERROR &&
           tls->last_error != ADB_ERR_OK)
            return tls->last_error;

        return ADB_ERR_GENERIC;
    }

    return ADB_ERR_OK;
}

static adb_error_t adb__write_tls(
        void *userdata,
        const void *buf,
        const size_t size)
{
    adb__conn_tls_t *tls = userdata;
    size_t offset = 0;
    int ret = 0;

    if(!tls || (!buf && size))
        return ADB_ERR_PARAM;

    while(offset < size)
    {
        ret = mbedtls_ssl_write(
                &tls->ssl,
                (const unsigned char *)buf + offset,
                size - offset);

        if(ret > 0)
        {
            offset += (size_t)ret;
            continue;
        }

        if(ret == MBEDTLS_ERR_SSL_INTERNAL_ERROR &&
           tls->last_error != ADB_ERR_OK)
            return tls->last_error;

        return ADB_ERR_GENERIC;
    }

    return ADB_ERR_OK;
}

typedef int (*adb__send_callback_t)(
        void *userdata,
        const uint8_t *buf,
        size_t size);

typedef int (*adb__recv_callback_t)(
        void *userdata,
        uint8_t *buf,
        size_t size);

static adb_error_t adb__tls_init(
        adb__conn_tls_t *tls,
        const adb__send_callback_t send_cb,
        const adb__recv_callback_t recv_cb,
        void *userdata)
{
    int err = 0;
    mbedtls_ssl_context ssl = {0};
    mbedtls_ssl_config conf = {0};
    mbedtls_ctr_drbg_context drbg = {0};
    mbedtls_entropy_context entropy = {0};

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ctr_drbg_init(&drbg);
    mbedtls_entropy_init(&entropy);

    err = mbedtls_ctr_drbg_seed(
            &drbg,
            mbedtls_entropy_func,
            &entropy,
            NULL, 0);
    if (err != 0)
    {
        adb__log_err_mbedtls("failed to create random generator", err);
        goto fail;
    }

    err = mbedtls_ssl_config_defaults(
            &conf,
            MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM,
            MBEDTLS_SSL_PRESET_DEFAULT);
    if (err != 0)
    {
        adb__log_err_mbedtls("failed to configure TLS defaults", err);
        goto fail;
    }

    mbedtls_ssl_conf_authmode(
            &conf,
            MBEDTLS_SSL_VERIFY_NONE);

    mbedtls_ssl_conf_rng(
            &conf,
            mbedtls_ctr_drbg_random,
            &drbg);

    err = mbedtls_ssl_setup(
            &ssl,
            &conf);
    if (err != 0)
    {
        adb__log_err_mbedtls("failed to setup TLS", err);
        goto fail;
    }

    mbedtls_ssl_set_bio(
            &ssl,
            userdata,
            send_cb,
            recv_cb,
            NULL);

    tls->ssl = ssl;
    tls->conf = conf;
    tls->drbg = drbg;
    tls->entropy = entropy;

    return ADB_ERR_OK;

fail:
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);
    return ADB_ERR_CRYPTO;
}

adb_error_t adb_conn_create_wireless(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const char *host,
        const uint16_t port)
{
    adb_error_t ret = ADB_ERR_OK;
    adb_conn_t *tmp = NULL;
    int sock = 0;
    struct sockaddr_in addr = {0};
    int err = 0;
    adb__conn_tls_t tls = {0};

    if(!conn || !ctx || !host || port == 0)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    ADB__INFO("creating tls connection to %s:%u", host, port);
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        ADB__ERROR("failed to create socket for %s:%u", host, port);
        ADB__INFO("reason: %s", strerror(errno));
        adb__free(tmp);
        return ADB_ERR_NETWORK;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if(inet_pton(AF_INET, host, &addr.sin_addr) != 1)
    {
        ADB__ERROR("failed to parse tls host %s", host);
        return ADB_ERR_NETWORK;
    }

    err = connect(sock,
            (struct sockaddr*)&addr, sizeof(addr));
    if(err < 0)
    {
        ADB__ERROR("failed to connect to %s:%u", host, port);
        ADB__INFO("reason: %s", strerror(errno));
    }

    ADB__INFO("connected to tls device %s:%u", host, port);
    ret = adb__tls_init(
            &tls, 
            adb__write_tcp, 
            adb__read_tcp, 
            (void*)(uintptr_t)sock);
    if(ret != ADB_ERR_OK)
        goto fail;

    tmp->ctx = ctx;

    tmp->type = ADB__CONN_TYPE_WIRELESS;
    tmp->profile = ADB_CONN_PROFILE_WIRELESS;
    tmp->tls = tls;

    tmp->libdata = &tmp->tls;
    tmp->read = adb__read_tls;
    tmp->write = adb__write_tls;
    *conn = tmp;

    ADB__INFO("created tls connection successfully to %s:%u",
            host, port);

    return ADB_ERR_OK;

fail:
    shutdown(sock, SHUT_RDWR);
    close(sock);
    adb__free(tmp);
    return ret;
}

static adb_error_t adb__read_custom(
        void *userdata,
        void *buf, 
        const size_t size)
{
}

static adb_error_t adb__write_custom(
        void *userdata, 
        const void *buf, 
        const size_t size)
{
}

#define ADB__CHECK_ENUM(val, pref) ((val) < 0 || (val) >= ADB__##pref##_COUNT)
{
}

adb_error_t adb_conn_create_custom(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        adb_read_callback_t read_cb,
        adb_write_callback_t write_cb,
        void *userdata,
        const adb_conn_profile_t profile)
{
    adb_conn_t *tmp = NULL;
    if(!conn || !read_cb || !write_cb || 
            ADB__CHECK_ENUM(profile, CONN_PROFILE))
        return ADB_ERR_PARAM;
    
    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    tmp->ctx = ctx;
    tmp->type = ADB__CONN_TYPE_CUSTOM;
    tmp->profile = profile;

    if(tmp->profile == ADB_CONN_PROFILE_WIRELESS)
    {
        adb__conn_tls_t tls = {0};
        adb_error_t res = ADB_ERR_OK;
        res = adb__tls_init(&tls, write_cb, read_cb, userdata);
        if(res != ADB_ERR_OK)
        {
            adb__free(tmp);
            return res;
        }
        
        tmp->tls = tls;
        tmp->libdata = &tmp->tls;
        tmp->read = adb__read_tls;
        tmp->write = adb__write_tls;
        *conn = tmp;
        return ADB_ERR_OK;
    }

    tmp->custom.read = read_cb;
    tmp->custom.write = write_cb;
    tmp->custom.userdata = userdata;
    tmp->libdata = &tmp->custom;
    tmp->read = adb__read_custom;
    tmp->write = adb__write_custom;

    return ADB_ERR_OK;
}

adb_error_t adb_conn_pair(
        adb_conn_t *conn,
        const char code[7])
{
    int err = 0;
    if(!conn || !code || strlen(code) != 6)
        return ADB_ERR_PARAM;
    if(conn->profile != ADB_CONN_PROFILE_WIRELESS)
        return ADB_ERR_UNSUPPORTED;

    err = mbedtls_ssl_handshake(&conn->tls.ssl);
    if(err != 0)
    {
        adb__log_err_mbedtls("failed to perform TLS handshake", err);
        return ADB_ERR_NETWORK;
    }


    return ADB_ERR_OK;
}

#define ADB__MAX_SUPPORTED_VER 0x01000001
#define ADB__MAX_PAYLOAD_SIZE (1024 * 1024)

static adb_error_t adb__send_packet(
        adb_conn_t *conn,
        adb__packet_t *pkt)
{
    const uint8_t *payload_data = NULL;
    uint32_t sum = 0;
    adb_error_t res = ADB_ERR_OK;
    if(!conn || !pkt)
        return ADB_ERR_PARAM;

    payload_data = pkt->payload;
    for(uint32_t i = 0; i < pkt->msg.data_length; i++)
        sum += payload_data[i];

    pkt->msg.data_check = sum;
    pkt->msg.magic = pkt->msg.command ^ 0xffffffff;

    res = conn->write(conn->libdata, &pkt->msg, sizeof(pkt->msg));
    if(res != ADB_ERR_OK)
        return res;

    if(pkt->msg.data_length == 0)
        return ADB_ERR_OK;

    res = conn->write(conn->libdata, pkt->payload, pkt->msg.data_length);
    if(res != ADB_ERR_OK)
        return res;

    return ADB_ERR_OK;
}

adb_error_t adb_conn_handshake(
        adb_conn_t *conn)
{
    const char conn_str[] = 
        "host::";
    adb__packet_t pkt = {0};
    if(!conn)
        return ADB_ERR_PARAM;

    pkt.msg.command = ADB__CMD_CNXN;
    pkt.msg.arg0 = ADB__MAX_SUPPORTED_VER;
    pkt.msg.arg1 = ADB__MAX_PAYLOAD_SIZE;
    pkt.msg.data_length = sizeof(conn_str) - 1;

    return adb__send_packet(conn, &pkt);
}

void adb_conn_destroy(
        adb_conn_t *conn)
{
    if(!conn)
        return;

    if(conn->profile == ADB_CONN_PROFILE_WIRELESS) 
    {
        mbedtls_ssl_free(&conn->tls.ssl);
        mbedtls_ssl_config_free(&conn->tls.conf);
        mbedtls_ctr_drbg_free(&conn->tls.drbg);
        mbedtls_entropy_free(&conn->tls.entropy);
    }

    if(conn->type == ADB__CONN_TYPE_WIRED)
        libusb_close(conn->usb.handle);
    else if(conn->type == ADB__CONN_TYPE_WIRELESS)
    {
        int sock = (int)(uintptr_t)conn->custom.userdata;
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }

    adb__free(conn);
}
