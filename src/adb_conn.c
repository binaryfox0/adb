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

#define ADB__INTERFACE_CLASS    0xFF
#define ADB__INTERFACE_SUBCLASS 0x42
#define ADB__INTERFACE_PROTOCOL 0x01

#define ADB__DEVICE_CLASS       0xDC
#define ADB__DEVICE_SUBCLASS    0x02

typedef struct adb_usb_info
{
    libusb_device *device;

    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t itf_idx;
    uint8_t read_ep;
    uint8_t write_ep;

    char manufacturer[256];
    char product[256];
    char serial[256];
} adb_usb_info_t;

typedef enum 
{
    ADB__CONN_WIRED,
    ADB__CONN_WIRELESS,
    ADB__CONN_CUSTOM
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

    int sock;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_context entropy;
} adb__conn_tls_t;

typedef struct adb_conn
{
    adb__conn_type_t type;

    union
    {
        adb__conn_usb_t usb;
        adb__conn_tls_t tls;
    };
            
    void *userdata;
    adb_read_callback_t read;
    adb_write_callback_t write;
} adb_conn_t;


const char *adb_conn_info_get_manufacturer(
        const adb_usb_info_t *conn_info) {
    return conn_info ? (const char *)conn_info->manufacturer : NULL;
}
const char *adb_conn_info_get_product(
        const adb_usb_info_t *conn_info) {
    return conn_info ? (const char *)conn_info->product : NULL;
}

static bool adb__find_adb_interface(
        libusb_device *device,
        uint8_t *itf_idx,
        uint8_t *read_ep,
        uint8_t *write_ep)
{
    int res = 0;
    struct libusb_config_descriptor *config = NULL;
    uint8_t bulk_in = 0;
    uint8_t bulk_out = 0;

    if(!device || !itf_idx || !read_ep || !write_ep)
        return false;

    res = libusb_get_active_config_descriptor(device, &config);
    if(res != 0)
    {
        ADB__WARN("failed to get active USB configuration");
        ADB__DEBUG("reason: %s (%s)",
                libusb_error_name(res),
                libusb_strerror(res));
        return false;
    }

    for(uint8_t i = 0; i < config->bNumInterfaces; i++)
    {
        const struct libusb_interface *itf = &config->interface[i];
        const struct libusb_interface_descriptor *itf_desc = NULL;
        bool found_in = false;
        bool found_out = false;

        if(itf->num_altsetting == 0)
            continue;

        itf_desc = &itf->altsetting[0];

        if(!(itf_desc->bInterfaceProtocol == ADB__INTERFACE_PROTOCOL &&
                (
                    (itf_desc->bInterfaceClass == ADB__INTERFACE_CLASS &&
                     itf_desc->bInterfaceSubClass == ADB__INTERFACE_SUBCLASS) ||
                    (itf_desc->bInterfaceClass == ADB__DEVICE_CLASS &&
                     itf_desc->bInterfaceSubClass == ADB__DEVICE_SUBCLASS)
                )))
        {
            continue;
        }

        for(int j = 0; j < itf_desc->bNumEndpoints; j++)
        {
            const struct libusb_endpoint_descriptor *endpoint =
                    &itf_desc->endpoint[j];

            if((endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK)
                    != LIBUSB_TRANSFER_TYPE_BULK)
                continue;

            if((endpoint->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
                    == LIBUSB_ENDPOINT_IN && !found_in)
            {
                found_in = true;
                bulk_in = endpoint->bEndpointAddress;
            }

            if((endpoint->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
                    == LIBUSB_ENDPOINT_OUT && !found_out)
            {
                found_out = true;
                bulk_out = endpoint->bEndpointAddress;
            }
        }

        if(found_in && found_out)
        {
            *itf_idx = i;
            *read_ep = bulk_in;
            *write_ep = bulk_out;

            libusb_free_config_descriptor(config);
            return true;
        }
    }

    libusb_free_config_descriptor(config);
    return false;
}

static adb_error_t adb__append_conn_info(
        adb_ctx_t *ctx,
        libusb_device *device,
        const struct libusb_device_descriptor *desc,
        const uint8_t itf_idx,
        const uint8_t read_ep,
        const uint8_t write_ep)
{
    adb_usb_info_t *conn_info = NULL;
    struct libusb_device_handle *handle = NULL;
    unsigned char manufacturer[256] = {0};
    unsigned char product[256] = {0};
    unsigned char serial[256] = {0};
    int res = 0;

    if(!ctx || !device || !desc)
        return ADB_ERR_PARAM;

    if(ctx->infos_count < ctx->infos_capacity)
    {
        conn_info = ctx->conn_infos[ctx->infos_count];
        if(conn_info)
        {
            ADB__DEBUG("reusing connection info slot %zu",
                    ctx->infos_count);
            memset(conn_info, 0, sizeof(*conn_info));
        }
        else
        {
            conn_info = adb__calloc(1, sizeof(*conn_info));
            if(!conn_info)
                return ADB_ERR_NO_MEM;
            ctx->conn_infos[ctx->infos_count] = conn_info;
        }
    }
    else
    {
        size_t old_capacity = 0;
        size_t new_capacity = 0;
        adb_usb_info_t **new_infos = NULL;

        old_capacity = ctx->infos_capacity;        
        new_capacity = ctx->infos_capacity == 0
                ? 8 : ctx->infos_capacity * 2;

        new_infos = adb__realloc(
                ctx->conn_infos, new_capacity * sizeof(*new_infos));

        if(!new_infos)
            return ADB_ERR_NO_MEM;

        memset(new_infos + old_capacity, 0,
                (new_capacity - old_capacity) * sizeof(*new_infos));

        ctx->conn_infos = new_infos;
        ctx->infos_capacity = new_capacity;
        conn_info = adb__calloc(1, sizeof(*conn_info));
        if(!conn_info)
            return ADB_ERR_NO_MEM;

        ctx->conn_infos[ctx->infos_count] = conn_info;
        ADB__DEBUG("allocated connection info slot %zu",
                ctx->infos_count);
    }

    res = libusb_open(device, &handle);
    if(res != 0)
    {
        ADB__WARN("failed to open USB device %04X:%04X",
                desc->idVendor,
                desc->idProduct);
        ADB__INFO("reason: %s (%s)",
                    libusb_error_name(res),
                    libusb_strerror(res));
        return ADB_ERR_USB;
    }

    if(desc->iManufacturer)
    {
        res = libusb_get_string_descriptor_ascii(
                handle,
                desc->iManufacturer,
                manufacturer,
                sizeof(manufacturer));

        if(res < 0)
        {
            ADB__DEBUG("failed to read manufacturer for %04X:%04X",
                    desc->idVendor,
                    desc->idProduct);
            ADB__INFO("reason: %s (%s)",
                    libusb_error_name(res),
                    libusb_strerror(res));
        }
    }

    if(desc->iProduct)
    {
        res = libusb_get_string_descriptor_ascii(
                handle,
                desc->iProduct,
                product,
                sizeof(product));

        if(res < 0)
        {
            ADB__DEBUG("failed to read product for %04X:%04X",
                    desc->idVendor,
                    desc->idProduct);
            ADB__INFO("reason: %s (%s)",
                    libusb_error_name(res),
                    libusb_strerror(res));
        }
    }

    if(desc->iSerialNumber)
    {
        res = libusb_get_string_descriptor_ascii(
                handle,
                desc->iSerialNumber,
                serial,
                sizeof(serial));

        if(res < 0)
        {
            ADB__DEBUG("failed to read serial for %04X:%04X",
                    desc->idVendor,
                    desc->idProduct);
            ADB__INFO("reason: %s (%s)",
                    libusb_error_name(res),
                    libusb_strerror(res));
        }
    }

    libusb_close(handle);

    if(!manufacturer[0] || !product[0])
    {
        char manufacturer_tmp[sizeof(manufacturer)] = {0};
        char product_tmp[sizeof(product)] = {0};

        ADB__WARN("USB device contain no manufacturer/product name, "
                "using usb.ids database");
        if(adb__lookup_usb(
                conn_info->vendor_id, conn_info->product_id, 
                manufacturer_tmp, sizeof(manufacturer_tmp),
                product_tmp, sizeof(product_tmp)))
        {
            ADB__INFO("found USB device inside usb.ids, using usb.ids "
                    "manufacturer/product name");
            memcpy(manufacturer, manufacturer_tmp, sizeof(manufacturer));
            memcpy(product, product_tmp, sizeof(product));
        } else {
            ADB__WARN("no USB device product was found "
                    "in usb.ids database, using libusb database");
        }
    }

    conn_info->device = libusb_ref_device(device);
    conn_info->vendor_id = desc->idVendor;
    conn_info->product_id = desc->idProduct;
    conn_info->itf_idx = itf_idx;
    conn_info->read_ep = read_ep;
    conn_info->write_ep = write_ep;

    snprintf((char *)conn_info->manufacturer, sizeof(conn_info->manufacturer),
            "%s", manufacturer[0] ? (char *)manufacturer : "unknown");
    snprintf((char *)conn_info->product, sizeof(conn_info->product),
            "%s", product[0] ? (char *)product : "unknown");
    snprintf((char *)conn_info->serial, sizeof(conn_info->serial),
            "%s", serial[0] ? (char *)serial : "unknown");

    ctx->infos_count++;
    ADB__INFO("found ADB device %04X:%04X %s %s (%s)",
            conn_info->vendor_id,
            conn_info->product_id,
            conn_info->manufacturer,
            conn_info->product,
            conn_info->serial);

    return ADB_ERR_OK;
}

void adb__conn_info_destroy(
        adb_usb_info_t *conn_info)
{
    if(!conn_info)
        return;
    libusb_unref_device(conn_info->device);
    adb__free(conn_info);
}

adb_error_t adb_query_usb(
        adb_ctx_t *ctx,
        adb_usb_info_t ***usb_infos,
        size_t *conn_count)
{
    adb_error_t ret = ADB_ERR_OK;
    int res = 0;
    libusb_device **list = NULL;
    ssize_t usb_count = 0;

    if(!ctx || !usb_infos || !conn_count)
        return ADB_ERR_PARAM;

    ADB__INFO("starting ADB device query");

    ctx->infos_count = 0;
    usb_count = libusb_get_device_list(ctx->usb, &list);
    if(usb_count < 0)
    {
        res = (int)usb_count;

        ADB__ERROR("failed to get USB device list: %s",
                libusb_error_name(res));
        ADB__DEBUG("reason: %s (%s)",
                libusb_error_name(res),
                libusb_strerror(res));

        return ADB_ERR_USB;
    }

    ADB__DEBUG("enumerated %zd USB device(s)", usb_count);
    for(ssize_t i = 0; i < usb_count; i++)
    {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc = {0};

        adb_error_t adb_res = ADB_ERR_OK;
        uint8_t itf_idx = 0;
        uint8_t read_ep = 0;
        uint8_t write_ep = 0;

        res = libusb_get_device_descriptor(device, &desc);
        if(res != 0)
        {
            ADB__WARN("failed to get USB device descriptor: %s",
                    libusb_error_name(res));
            continue;
        }

        if(desc.bDeviceClass != LIBUSB_CLASS_PER_INTERFACE)
            continue;

        if(!adb__find_adb_interface(
                device,
                &itf_idx,
                &read_ep,
                &write_ep))
            continue;

        ADB__DEBUG("ADB interface found on %04X:%04X "
                "(interface=%d, IN=0x%02X, OUT=0x%02X)",
                desc.idVendor,
                desc.idProduct,
                itf_idx,
                read_ep,
                write_ep);

        adb_res = adb__append_conn_info(ctx, 
                device, &desc,
                itf_idx,
                read_ep, write_ep);
        if(adb_res != ADB_ERR_OK)
        {
            ADB__ERROR("failed to add ADB device %04X:%04X",
                    desc.idVendor,
                    desc.idProduct);
            ret = adb_res;
            break;
        }
    }

    libusb_free_device_list(list, true);

    *usb_infos = ctx->conn_infos;
    *conn_count = ctx->infos_count;

    ADB__INFO("ADB device query completed: %zu device(s)",
            ctx->infos_count);

    return ret;
}

static adb_error_t adb__read_libusb(
        void *userdata,
        void *buf,
        size_t size)
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
        const void *data,
        size_t size)
{
    adb__conn_usb_t *usb = userdata;
    int transferred = 0;
    int ret = 0;

    ret = libusb_bulk_transfer(
            usb->handle,
            usb->write_ep,
            (uint8_t*)(uintptr_t)data,
            (int)size,
            &transferred,
            0);

    if(ret != LIBUSB_SUCCESS)
        return adb__error_from_libusb(ret);

    if(transferred != (int)size)
        return ADB_ERR_IO;

    return ADB_ERR_OK;
}

adb_error_t adb_conn_create_from_info(
        adb_conn_t **conn,
        const adb_usb_info_t *usb_info)
{
    adb_conn_t *tmp = NULL;
    int res = 0;
    if(!conn || !usb_info)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;
    tmp->type = ADB__CONN_WIRED; // for partial cleanup
                                 
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
                (unsigned char *)buf + offset,
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

static adb_error_t adb__tls_init(
        adb__conn_tls_t *tls)
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
        char buf[256] = {0};
        mbedtls_strerror(err, buf, sizeof(buf));
        ADB__ERROR("failed to create random generator");
        ADB__INFO("reason: %s", buf);
        return ADB_ERR_CRYPTO;
    }

    err = mbedtls_ssl_config_defaults(
            &tls->conf,
            MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM,
            MBEDTLS_SSL_PRESET_DEFAULT);
    if (err != 0)
    {
        char buf[256] = {0};
        mbedtls_strerror(err, buf, sizeof(buf));
        ADB__ERROR("failed to configure TLS defaults");
        ADB__INFO("reason: %s", buf);
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
        char buf[256] = {0};
        mbedtls_strerror(err, buf, sizeof(buf));
        ADB__ERROR("failed to setup TLS");
        ADB__INFO("reason: %s", buf);
        return ADB_ERR_CRYPTO;
    }

    mbedtls_ssl_set_bio(
            &tls->ssl,
            tls,
            adb__write_tcp,
            adb__read_tcp,
            NULL);

    return ADB_ERR_OK;
}

adb_error_t adb_conn_create_wireless(
        adb_conn_t **conn,
        const char *host,
        const uint16_t port)
{
    adb_error_t ret = ADB_ERR_OK;
    adb_conn_t *tmp = NULL;
    adb__conn_tls_t *tls = NULL;
    int err = 0;
    struct sockaddr_in addr = {0};

    if(!conn || !host || port == 0)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;
    tls = &tmp->tls;
    tmp->type = ADB__CONN_WIRELESS; // for partial cleanup

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
    ret = adb__tls_init(&tmp->tls);
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

adb_error_t adb_conn_create_custom(
        adb_conn_t **conn,
        const adb_read_callback_t read_cb,
        const adb_write_callback_t write_cb,
        void *userdata)
{
    adb_conn_t *tmp = NULL;
    if(!conn || !read_cb || !write_cb)
        return ADB_ERR_PARAM;
    
    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    tmp->type = ADB__CONN_CUSTOM;
    tmp->read = read_cb;
    tmp->write = write_cb;
    tmp->userdata = userdata;
    return ADB_ERR_OK;
}

adb_error_t adb_conn_pair(
        adb_conn_t *conn,
        const char code[7])
{
    if(!conn || !code || strlen(code) != 6)
        return ADB_ERR_PARAM;
    if(
            conn->type != ADB__CONN_WIRELESS && 
            conn->type != ADB__CONN_CUSTOM)
        return ADB_ERR_UNSUPPORTED;


    if(conn->type == ADB__CONN_WIRELESS)
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

    res = conn->write(conn->userdata, &pkt->msg, sizeof(pkt->msg));
    if(res != ADB_ERR_OK)
        return res;

    if(pkt->msg.data_length == 0)
        return ADB_ERR_OK;

    res = conn->write(conn->userdata, pkt->payload, pkt->msg.data_length);
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

    if(conn->type == ADB__CONN_WIRED)
    {
        libusb_close(conn->usb.handle);
    } else if(conn->type == ADB__CONN_WIRELESS) {
        mbedtls_ssl_free(&conn->tls.ssl);
        mbedtls_ssl_config_free(&conn->tls.conf);
        mbedtls_ctr_drbg_free(&conn->tls.drbg);
        mbedtls_entropy_free(&conn->tls.entropy);

        shutdown(conn->tls.sock, SHUT_RDWR);
        close(conn->tls.sock);
    }
    adb__free(conn);
}
