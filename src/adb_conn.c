#include <adb/adb_conn.h>
#include "adb_conn_priv.h"

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
#include "adb_lookup.h"
#include "adb_error_priv.h"
#include "adb_packet.h"

typedef struct
{
    libusb_device_handle *handle;
    uint8_t read_ep;
    uint8_t write_ep;
} adb__conn_usb_t;

typedef struct
{
    adb_error_t last_error;

    int sock;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_context entropy;
} adb__conn_tls_t;

typedef struct adb_conn
{
    adb_ctx_t *ctx;
    adb_conn_type_t type;

    union
    {
        adb__conn_usb_t usb;
        adb__conn_tls_t tls;
    };

    struct
    {
        adb_read_callback_t read;
        adb_write_callback_t write;
        void *userdata;
    } custom;
            
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


const char *adb_wired_info_manufacturer(
        const adb_usb_info_t *conn_info) {
    return conn_info ? (const char *)conn_info->manufacturer : NULL;
}
const char *adb_wired_info_product(
        const adb_usb_info_t *conn_info) {
    return conn_info ? (const char *)conn_info->product : NULL;
}


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
        const adb_usb_info_t *usb_info)
{
    adb_conn_t *tmp = NULL;
    int res = 0;
    if(!conn || !ctx || !usb_info)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;
    tmp->type = ADB_CONN_TYPE_WIRED; // for partial cleanup
                                 
    ADB__INFO("creating connection for USB device %04X:%04X %s %s",
            usb_info->vendor_id, usb_info->product_id,
            usb_info->manufacturer, usb_info->product);
    
    res = libusb_open(usb_info->device, &tmp->usb.handle);
    if(res != 0)
    {
        ADB__ERROR("failed to open USB device %04X:%04X %s %s",
                usb_info->vendor_id, usb_info->product_id,
                usb_info->manufacturer, usb_info->product);
        ADB__INFO("reason: %s (%s)",
                    libusb_error_name(res),
                    libusb_strerror(res));
        adb__free(tmp);
        return ADB_ERR_USB;
    }

    res = libusb_claim_interface(tmp->usb.handle, 
            usb_info->itf_idx);
    if(res != 0)
    {
        ADB__ERROR("failed to claim USB device interface %u: %04X:%04X %s %s",
                usb_info->itf_idx,
                usb_info->vendor_id, usb_info->product_id,
                usb_info->manufacturer, usb_info->product);
        ADB__INFO("reason: %s (%s)",
                    libusb_error_name(res),
                    libusb_strerror(res));
        adb_conn_destroy(tmp);
        return ADB_ERR_USB;
    }

    tmp->libdata = &tmp->usb;
    tmp->read = adb__read_libusb;
    tmp->write = adb__write_libusb;
    *conn = tmp;

    ADB__INFO("created connection successfully for USB device "
            "%04X:%04X %s %s (interface %u)",
            usb_info->vendor_id, usb_info->product_id,
            usb_info->manufacturer, usb_info->product,
            usb_info->itf_idx);
    return ADB_ERR_OK;
}


static int adb__read_tcp(
        void *userdata,
        unsigned char *buf,
        size_t size)
{
    adb__conn_tls_t *tls = userdata;
    ssize_t res = 0;

    res = recv(
            tls->sock,
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

    mbedtls_ssl_init(&tls->ssl);
    mbedtls_ssl_config_init(&tls->conf);
    mbedtls_ctr_drbg_init(&tls->drbg);
    mbedtls_entropy_init(&tls->entropy);

    err = mbedtls_ctr_drbg_seed(
            &tls->drbg,
            mbedtls_entropy_func,
            &tls->entropy,
            NULL, 0);
    if (err != 0)
    {
        ADB__ERROR("failed to create random generator");
        return ADB_ERR_CRYPTO;
    }

    err = mbedtls_ssl_config_defaults(
            &tls->conf,
            MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM,
            MBEDTLS_SSL_PRESET_DEFAULT);
    if (err != 0)
    {
        ADB__ERROR("failed to configure TLS defaults");
        return ADB_ERR_CRYPTO;
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
    if (err != 0)
    {
        ADB__ERROR("failed to setup TLS");
        return ADB_ERR_CRYPTO;
    }

    mbedtls_ssl_set_bio(
            &tls->ssl,
            userdata,
            send_cb,
            recv_cb,
            NULL);

    return ADB_ERR_OK;
}

adb_error_t adb_conn_create_wireless(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const char *host,
        const uint16_t port)
{
    adb_error_t ret = ADB_ERR_OK;
    adb_conn_t *tmp = NULL;
    adb__conn_tls_t *tls = NULL;
    int err = 0;
    struct sockaddr_in addr = {0};

    if(!conn || !ctx || !host || port == 0)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;
    tls = &tmp->tls;
    tmp->type = ADB_CONN_TYPE_WIRELESS; // for partial cleanup

    ADB__INFO("creating tls connection to %s:%u", host, port);

    tls->sock = socket(AF_INET, SOCK_STREAM, 0);
    if(tls->sock < 0)
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
        adb_conn_destroy(tmp);
        return ADB_ERR_NETWORK;
    }

    err = connect(tls->sock,
            (struct sockaddr*)&addr, sizeof(addr));
    if(err < 0)
    {
        ADB__ERROR("failed to connect to %s:%u", host, port);
        ADB__INFO("reason: %s", strerror(errno));
        adb_conn_destroy(tmp);
        return ADB_ERR_NETWORK;
    }

    ADB__INFO("connected to tls device %s:%u", host, port);
    ret = adb__tls_init(
            &tmp->tls, 
            adb__write_tcp, 
            adb__read_tcp, 
            tls);
    if(ret != ADB_ERR_OK)
    {
        adb_conn_destroy(tmp);
        return ADB_ERR_NETWORK;
    }

    tmp->read = adb__read_tls;
    tmp->write = adb__write_tls;
    *conn = tmp;

    ADB__INFO("created tls connection successfully to %s:%u",
            host, port);

    return ADB_ERR_OK;
}

static adb_error_t adb__read_custom(
        void *userdata)
{
}
#define ADB__CHECK_ENUM(val, pref) ((val) < 0 || (val) >= ADB__##pref##_COUNT)
adb_error_t adb__write_custom

adb_error_t adb_conn_create_custom(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const adb_read_callback_t read_cb,
        const adb_write_callback_t write_cb,
        void *userdata,
        const adb_conn_type_t type)
{
    adb_conn_t *tmp = NULL;
    if(!conn || !read_cb || !write_cb || ADB__CHECK_ENUM(type, CONN_TYPE))
        return ADB_ERR_PARAM;
    
    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    tmp->type = ADB_CONN_TYPE_CUSTOM;
    tmp->custom.read = read_cb;
    tmp->custom.write = write_cb;
    tmp->custom.userdata = userdata;

    return ADB_ERR_OK;
}

adb_error_t adb_conn_pair(
        adb_conn_t *conn,
        const char code[7])
{
    if(!conn || !code || strlen(code) != 6)
        return ADB_ERR_PARAM;
    if(
            conn->type != ADB_CONN_TYPE_WIRELESS && 
            conn->type != ADB_CONN_TYPE_CUSTOM)
        return ADB_ERR_UNSUPPORTED;


    if(conn->type == ADB_CONN_TYPE_WIRELESS)
    {
        int err = mbedtls_ssl_handshake(&conn->tls.ssl);
        if(err != 0)
        {
            char buf[256] = {0};
            mbedtls_strerror(err, buf, sizeof(buf));
            ADB__ERROR("failed to perform TLS handshake");
            ADB__INFO("reason: %s", buf);
            return ADB_ERR_NETWORK;
        }
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

    if(conn->type == ADB_CONN_TYPE_WIRED)
    {
        libusb_close(conn->usb.handle);
    } else if(conn->type == ADB_CONN_TYPE_WIRELESS) {
        mbedtls_ssl_free(&conn->tls.ssl);
        mbedtls_ssl_config_free(&conn->tls.conf);
        mbedtls_ctr_drbg_free(&conn->tls.drbg);
        mbedtls_entropy_free(&conn->tls.entropy);

        shutdown(conn->tls.sock, SHUT_RDWR);
        close(conn->tls.sock);
    }
    adb__free(conn);
}
