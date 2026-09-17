#include <adb/adb_conn.h>
#include "adb_conn_priv.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <libusb.h>
#include <mbedtls/error.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#include "adb_utils.h"
#include "adb_log_priv.h"
#include "adb_ctx_priv.h"
#include "adb_alloc_priv.h"
#include "adb_query_priv.h"
#include "adb_packet.h"
#include "adb_key_priv.h"
#include "adb_transport.h"
#include "adb_tls.h"
#include "adb_str.h"

#include "adb_transport_usb.h"
#include "adb_transport_tcp.h"
#include "adb_transport_custom.h"

typedef enum
{
    ADB__CONN_STATE_BOOTLOADER,
    ADB__CONN_STATE_DEVICE,
    ADB__CONN_STATE_RECOVERY,
    ADB__CONN_STATE_SIDELOAD,
    ADB__CONN_STATE_RESCUE,
    ADB__CONN_STATE_HOST,
    ADB__CONN_STATE_COUNT
} adb__conn_state_t;

typedef struct adb_conn
{
    adb_ctx_t *ctx;
    adb_conn_profile_t profile;

    adb__transport_t transport;
    adb__tls_t tls;

    uint32_t max_payload_size;
    adb__conn_state_t state;
    
    uint8_t *banner;
    adb__str_t name;
    adb__str_t model;
    adb__str_t device;
    adb__str_t features;
} adb_conn_t;

static const char *adb__conn_state_readable[ADB__CONN_STATE_COUNT] =
{
    [ADB__CONN_STATE_BOOTLOADER]    = "bootloader",
    [ADB__CONN_STATE_DEVICE]        = "device",
    [ADB__CONN_STATE_RECOVERY]      = "recovery",
    [ADB__CONN_STATE_SIDELOAD]      = "sideload",
    [ADB__CONN_STATE_RESCUE]        = "rescue",
    [ADB__CONN_STATE_HOST]          = "host"
};

static adb_error_t adb__conn_post_init(
        adb_conn_t *conn)
{
    if(!conn)
        return ADB_ERR_PARAM;

    conn->max_payload_size = ADB__PACKET_MAX_PAYLOAD_SIZE;
    return ADB_ERR_OK;
}

adb_error_t adb_conn_create_wired(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const adb_wired_info_t *info)
{
    adb_conn_t *tmp = NULL;
    adb_error_t res = ADB_ERR_OK;

    if(!conn || !ctx || !info)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    tmp->ctx = ctx;
    tmp->profile = ADB_CONN_PROFILE_WIRED;

    res = adb__usb_transport_create(
            &tmp->transport,
            info);
    if(res != ADB_ERR_OK)
        goto fail;

    res = adb__conn_post_init(tmp);
    if(res != ADB_ERR_OK)
        goto fail;

    *conn = tmp;
    return ADB_ERR_OK;

fail:
    adb_conn_destroy(tmp);
    return res;
}

adb_error_t adb__conn_from_sockaddr(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const struct sockaddr *addr)
{
    adb_conn_t *tmp = NULL;
    adb_error_t res = ADB_ERR_OK;

    if(!conn || !ctx || !addr)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    tmp->ctx = ctx;
    tmp->profile = ADB_CONN_PROFILE_WIRELESS;

    res = adb__tcp_transport_create(
            &tmp->transport, addr);
    if(res != ADB_ERR_OK)
        goto fail;
    
    res = adb__conn_post_init(tmp);
    if(res != ADB_ERR_OK)
        goto fail;

    *conn = tmp;
    return ADB_ERR_OK;

fail:
    adb_conn_destroy(tmp);
    return res;
}


adb_error_t adb_conn_create_wireless(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const char *host,
        uint16_t port)
{
    struct sockaddr addr = {0};
    if(!conn || !ctx || !host || port == 0)
        return ADB_ERR_PARAM;

    if(!adb__tcp_sockaddr_from_host_port(&addr, host, port))
        return ADB_ERR_PARAM;
    return adb__conn_from_sockaddr(conn, ctx, &addr);
}

adb_error_t adb_conn_create_custom(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const adb_conn_read_fn read_cb,
        const adb_conn_write_fn write_cb,
        void *userdata,
        const adb_conn_profile_t profile)
{
    adb_conn_t *tmp = NULL;
    adb_error_t res = ADB_ERR_OK;

    if(!conn || !ctx ||
       !read_cb ||
       !write_cb ||
       !ADB__CHECK_ENUM(profile, CONN_PROFILE))
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    tmp->ctx = ctx;
    tmp->profile = profile;

    res = adb__custom_transport_create(
            &tmp->transport,
            read_cb,
            write_cb,
            userdata);
    if(res != ADB_ERR_OK)
        goto fail;

    res = adb__conn_post_init(tmp);
    if(res != ADB_ERR_OK)
        goto fail;

    *conn = tmp;
    return ADB_ERR_OK;

fail:
    adb__free(tmp);
    return res;
}

adb_error_t adb__conn_upgrade_tls(
        adb_conn_t *conn,
        adb_key_t *key)
{
    adb_error_t res = ADB_ERR_OK;
    if(!conn)
        return ADB_ERR_PARAM;

    res = adb__tls_init(
            &conn->tls,
            conn->ctx,
            key,
            &conn->transport);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__tls_handshake(&conn->tls);
    if(res != ADB_ERR_OK)
    {
        adb__tls_destroy(&conn->tls);
        return res;
    }
    
    return ADB_ERR_OK;
}

#define ADB__STLS_VERSION 0x01000000
#define ADB__STLS_MIN_VERSION 0x01000000


static bool adb__parse_banner(
        adb_conn_t *conn,
        const uint8_t *banner,
        const size_t banner_len)
{
    adb__str_t curr = {0};
    adb__str_t type = {0};
    adb__str_t props = {0};
    adb__str_t property = {0};


    curr.ptr = ADB__CONST_CAST(char*, banner);
    curr.len = banner_len;

    if(!adb__str_next_tok(&curr, ':', &type))
        goto fail;
    /* skip empty token */
    if(!adb__str_next_tok(&curr, ':', NULL))
        goto fail;
    if(!adb__str_next_tok(&curr, ':', &props))
        goto fail;

    while(adb__str_next_tok(&props, ';', &property)) 
    {
        size_t idx = 0;
        adb__str_t key = {0};
        adb__str_t value = {0};

        if(adb__str_is_empty(&property))
        {
            ADB__WARN("ignoring empty device property");
            continue;
        }

        idx = adb__str_find_char(&property, '=');
        if(idx == ADB__STR_NPOS)
        {
            ADB__WARN("ignoring malformed device property");
            ADB__INFO("property has no '=' separator");
            continue;
        }

        key.ptr = property.ptr;
        key.len = idx;

        value.ptr = property.ptr + idx + 1;
        value.len = property.len - idx - 1;

        if(adb__str_find_char(&value, '=') != ADB__STR_NPOS)
        {
            ADB__WARN("ignoring malformed device property");
            ADB__INFO("property value contains multiple '=' characters");
            continue;
        }

        if(adb__str_compare_cstr(&key, "ro.product.name"))
            conn->name = value;
        else if(adb__str_compare_cstr(&key, "ro.product.model"))
            conn->model = value;
        else if(adb__str_compare_cstr(&key, "ro.product.device"))
            conn->device = value;
        else if(adb__str_compare_cstr(&key, "features"))
            conn->features = value;
        else
        {
            ADB__WARN("ignoring unknown device property");
            ADB__INFO("property name=\"%.*s\", value=\"%.*s\"", 
                    ADB__STR_PRINTF_EXPAND(&key),
                    ADB__STR_PRINTF_EXPAND(&value));
        }
    }

    conn->state = ADB__CONN_STATE_COUNT;
    for(size_t i = 0; i < ADB__ARRSZ(adb__conn_state_readable); i++)
    {
        if(adb__str_compare_cstr(&type, adb__conn_state_readable[i]))
        {
            conn->state = (adb__conn_state_t)i;
            break;
        }
    }

    if(conn->state == ADB__CONN_STATE_COUNT)
    {
        ADB__WARN("unknown device connection state; falling back to host");
        ADB__INFO("connection state: \"%.*s\"", ADB__STR_PRINTF_EXPAND(&type));
        conn->state = ADB__CONN_STATE_HOST;
    }

    ADB__DEBUG("banner: name=\"%.*s\", model=\"%.*s\", "
            "device=\"%.*s\", features=\"%.*s\"",
            ADB__STR_PRINTF_EXPAND(&conn->name),
            ADB__STR_PRINTF_EXPAND(&conn->model),
            ADB__STR_PRINTF_EXPAND(&conn->device),
            ADB__STR_PRINTF_EXPAND(&conn->features));

    return true;

fail:
    ADB__ERROR("failed to parse device banner");
    return false;
}

static adb_error_t adb__conn_handle_stls(
        adb_conn_t *conn, 
        adb_key_t *key)
{
    static const adb__packet_t pkt = 
    {
        .command = ADB__CMD_STLS,
        .arg0 = ADB__STLS_VERSION,
        .arg1 = 0,
        .payload_size = 0,
    };

    adb_error_t ret = adb__packet_write(conn, &pkt, NULL);
    if (ret != ADB_ERR_OK)
        return ret;

    return adb__conn_upgrade_tls(conn, key);
}

adb_error_t adb_conn_handshake(
        adb_conn_t *conn, 
        adb_key_t *key)
{
    static const char conn_str[] = "host::";
    adb__packet_t pkt = 
    {
        .command = ADB__CMD_CNXN,
        .arg0 = ADB__PACKET_MAX_SUPPORTED_VER,
        .arg1 = ADB__PACKET_MAX_PAYLOAD_SIZE,
        .payload_size = sizeof(conn_str) - 1,
    };

    uint8_t *payload = NULL;
    adb_error_t ret;

    if (!conn || !key)
        return ADB_ERR_PARAM;

    if(
            (ret = adb__packet_write(conn, &pkt, conn_str)) != ADB_ERR_OK ||
            (ret = adb__packet_read(conn, &pkt, 
                                    (void **)&payload) != ADB_ERR_OK))
        return ret;

    if (pkt.command == ADB__CMD_STLS) 
    {
        ret = adb__conn_handle_stls(conn, key);
        if (ret != ADB_ERR_OK)
            goto fail;

        adb__free(payload); payload = NULL;
        ret = adb__packet_read(conn, &pkt, (void **)&payload);
        if (ret != ADB_ERR_OK)
            goto fail;
    }

    if(
            !adb__packet_check_cmd(&pkt, ADB__CMD_CNXN) ||
            !adb__parse_banner(conn, payload, pkt.payload_size))
    {
        ret = ADB_ERR_PROTOCOL;
        goto fail;
    }

    conn->banner = payload;
    return ADB_ERR_OK;

fail:
    adb__free(payload);
    return ret;
}

void adb_conn_destroy(
        adb_conn_t *conn)
{
    if(!conn)
        return;

    adb__tls_destroy(&conn->tls);
    adb__transport_destroy(&conn->transport);
    adb__free(conn->banner);
    adb__free(conn);
}

adb_error_t adb__conn_read(
        adb_conn_t *conn,
        void *buf,
        const size_t size)
{
    adb_error_t res = ADB_ERR_OK;
    if(!conn)
        return ADB_ERR_PARAM;

    if(conn->tls.initialized)
    {
        res = adb__tls_read(
                &conn->tls,
                buf,
                size);
    } else {
        res = adb__transport_read(
                &conn->transport,
                buf,
                size);
    }
    
    if(res == ADB_ERR_OK)
        adb__log_payload(buf, size, "read data");
    return res;
}


adb_error_t adb__conn_write(
        adb_conn_t *conn,
        const void *buf,
        const size_t size)
{
    if(!conn)
        return ADB_ERR_PARAM;

    adb__log_payload(buf, size, "write data");
    if(conn->tls.initialized)
    {
        return adb__tls_write(
                &conn->tls,
                buf,
                size);
    } else {
        return adb__transport_write(
                &conn->transport,
                buf,
                size);
    }
}

uint32_t adb__conn_get_max_payload_size(
        adb_conn_t *conn) {
    return conn ? conn->max_payload_size : 0;
}

adb__tls_t *adb__conn_get_tls(
        adb_conn_t *conn) {
    return conn ? &conn->tls : NULL;
}

adb_ctx_t *adb__conn_get_ctx(
        adb_conn_t *conn) {
    return conn ? conn->ctx : NULL;
}

