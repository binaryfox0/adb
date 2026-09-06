#include <adb/adb_conn.h>
#include "adb_conn_priv.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

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
#include "adb_key_priv.h"

#include "adb_transport.h"
#include "adb_tls.h"
#include "adb_transport_usb.h"
#include "adb_transport_tcp.h"
#include "adb_transport_custom.h"

struct adb_conn
{
    adb_ctx_t *ctx;
    adb_conn_profile_t profile;

    adb__transport_t transport;
    adb__tls_t tls;
};

adb_error_t adb_conn_create_wired(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const adb_wired_info_t *info)
{
    adb_conn_t *tmp = NULL;
    adb_error_t err = ADB_ERR_OK;

    if(!conn || !ctx || !info)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    tmp->ctx = ctx;
    tmp->profile = ADB_CONN_PROFILE_WIRED;

    err = adb__usb_transport_create(
            &tmp->transport,
            info);

    if(err != ADB_ERR_OK)
    {
        adb__free(tmp);
        return err;
    }

    *conn = tmp;
    return ADB_ERR_OK;
}

adb_error_t adb_conn_create_wireless(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const char *host,
        uint16_t port)
{
    adb_conn_t *tmp = NULL;
    adb_error_t err = ADB_ERR_OK;

    if(!conn || !ctx || !host || port == 0)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    tmp->ctx = ctx;
    tmp->profile = ADB_CONN_PROFILE_WIRELESS;

    err = adb__tcp_transport_create(
            &tmp->transport,
            host, port);

    if(err != ADB_ERR_OK)
        goto fail;

    err = adb__tls_init(
            &tmp->tls,
            &tmp->transport);

    if(err != ADB_ERR_OK)
        goto fail;

    *conn = tmp;
    return ADB_ERR_OK;

fail:

    adb__tls_destroy(&tmp->tls);
    adb__transport_destroy(&tmp->transport);
    adb__free(tmp);

    return err;
}

#define ADB__CHECK_ENUM(val, pref) ((val) < 0 || (val) >= ADB__##pref##_COUNT)

adb_error_t adb_conn_create_custom(
        adb_conn_t **conn,
        adb_ctx_t *ctx,
        const adb_conn_read_fn read_cb,
        const adb_conn_write_fn write_cb,
        void *userdata,
        const adb_conn_profile_t profile)
{
    adb_conn_t *tmp = NULL;
    adb_error_t err = ADB_ERR_OK;

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

    err = adb__custom_transport_create(
            &tmp->transport,
            read_cb,
            write_cb,
            userdata);

    if(err != ADB_ERR_OK)
    {
        adb__free(tmp);
        return err;
    }

    if(profile == ADB_CONN_PROFILE_WIRELESS)
    {
        err = adb__tls_init(
                &tmp->tls,
                &tmp->transport);

        if(err != ADB_ERR_OK)
        {
            adb__transport_destroy(
                    &tmp->transport);

            adb__free(tmp);
            return err;
        }
    }

    *conn = tmp;
    return ADB_ERR_OK;
}

static inline bool adb__verify_pairing_code(
        const char *code,
        const size_t code_len)
{
    if(!code && code_len != 6)
        return false;
    for(size_t i = 0; i < code_len; i++)
    {
        if(!isdigit(code[i]))
            return false;
    }
    return true;
}

static void adb__print_keying_material(
        const uint8_t *keying_material)
{
    char buffer[ADB__TLS_EXPORTED_KEY_SIZE * 2 + 1] = {0};
    if(!keying_material)
        return;

    for(int i = 0; i < ADB__TLS_EXPORTED_KEY_SIZE; i++)
    {
        snprintf(buffer + i * 2, sizeof(buffer) - (size_t)i * 2, 
                "%02x", keying_material[i]);
    }
    ADB__DEBUG("keying material: \"%s\"", buffer);
}

adb_error_t adb_conn_pair(
        adb_conn_t *conn,
        const char *code,
        const size_t code_len,
        adb_key_t *key)
{
    adb_error_t res = ADB_ERR_OK;
    uint8_t keying_material[ADB__TLS_EXPORTED_KEY_SIZE] = {0};
    uint8_t private_key[4096] = {0};
    uint8_t x509_cert[4096] = {0};

    if(!conn || !adb__verify_pairing_code(code, code_len))
        return ADB_ERR_PARAM;
    if(!conn->tls.initialized)
        return ADB_ERR_UNSUPPORTED;

    ADB__INFO("pairing wireless device with code \"%6s\"", code);

    
    res = adb__tls_handshake(&conn->tls);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__tls_export_keying_material(
            &conn->tls,
            keying_material);
    if(res != ADB_ERR_OK)
        return res;

    adb__tls_export_keying_material(&conn->tls, keying_material);
    adb__key_write_x509_pem(key, conn->ctx, x509_cert, sizeof(x509_cert));
    adb__key_write_pkcs8_pem(key, private_key, sizeof(private_key));

    adb__print_keying_material(keying_material);

    ADB__INFO("pairing wireless device successfully");
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

    res = adb__conn_write(conn, &pkt->msg, sizeof(pkt->msg));
    if(res != ADB_ERR_OK)
        return res;

    if(pkt->msg.data_length == 0)
        return ADB_ERR_OK;

    res = adb__conn_write(conn, pkt->payload, pkt->msg.data_length);
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

    adb__tls_destroy(&conn->tls);
    adb__transport_destroy(&conn->transport);
    adb__free(conn);
}

adb_error_t adb__conn_read(
        adb_conn_t *conn,
        void *buf,
        const size_t size)
{
    if(!conn)
        return ADB_ERR_PARAM;

    if(conn->tls.initialized)
    {
        return adb__tls_read(
                &conn->tls,
                buf,
                size);
    }

    return adb__transport_read(
            &conn->transport,
            buf,
            size);
}


adb_error_t adb__conn_write(
        adb_conn_t *conn,
        const void *buf,
        const size_t size)
{
    if(!conn)
        return ADB_ERR_PARAM;

    if(conn->tls.initialized)
    {
        return adb__tls_write(
                &conn->tls,
                buf,
                size);
    }

    return adb__transport_write(
            &conn->transport,
            buf,
            size);
}
