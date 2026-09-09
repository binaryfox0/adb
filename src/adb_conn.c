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
#include <spake2.h>

#include "adb/adb_error.h"
#include "adb_log_priv.h"
#include "adb_ctx_priv.h"
#include "adb_alloc_priv.h"
#include "adb_error_priv.h"
#include "adb_query_priv.h"
#include "adb_packet.h"
#include "adb_key_priv.h"
#include "adb_utils.h"

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

#define ADB__PAIRING_CODE_DIGITS 6
static inline bool adb__verify_pairing_code(
        const char *code,
        const size_t code_len)
{
    if(!code && code_len != ADB__PAIRING_CODE_DIGITS)
        return false;
    for(size_t i = 0; i < code_len; i++)
    {
        if(!isdigit(code[i]))
            return false;
    }
    return true;
}

typedef struct __attribute__((packed))
{
    uint8_t version;   // PairingPacket version
    uint8_t type;      // the type of packet (PairingPacket.Type)
    uint32_t payload_size;  // Size of the payload in bytes
} adb__pairing_header_t;

#define ADB__PAIRING_HEADER_VER     1
#define ADB__PAIRING_HEADER_MIN_VER 1
#define ADB__PAIRING_HEADER_MAX_VER 1
#define ADB__MAX_PEER_INFO_SIZE     8192
#define ADB__MAX_PAIRING_PAYLOAD_SIZE       (ADB__MAX_PEER_INFO_SIZE * 2)

enum 
{
    ADB__PAIRING_SPAKE2_MSG,
    ADB__PAIRING_PEER_INFO,
    ADB__PAIRING_COUNT,
};

static adb_error_t adb__read_pairing_header(
        adb_conn_t *conn,
        adb__pairing_header_t *header)
{
    static const size_t version_offset =
        offsetof(adb__pairing_header_t, version);
    static const size_t type_offset =
        offsetof(adb__pairing_header_t, type);
    static const size_t plsz_offset = 
        offsetof(adb__pairing_header_t, payload_size);
    uint8_t buffer[sizeof(*header)] = {0};
    adb_error_t res = ADB_ERR_OK;
    uint8_t version = 0;
    uint8_t type = 0;

    if(!conn || !header)
        return ADB_ERR_PARAM;

    ADB__INFO("reading pairing header");

    res = adb__conn_read(conn, buffer, sizeof(buffer));
    if(res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to read pairing header");
        return res;
    }
    /*
    adb__log_print_payload(
            buffer, sizeof(buffer), 
            "received pairing header");
    */
    version = buffer[version_offset];
    if(!ADB__IN_RANGE(version, 
                ADB__PAIRING_HEADER_MIN_VER, 
                ADB__PAIRING_HEADER_MAX_VER))
    {
        ADB__ERROR("unsupported pairing header version");
        ADB__INFO("supported version range %d,%d, got: %d",
                ADB__PAIRING_HEADER_MIN_VER,
                ADB__PAIRING_HEADER_MAX_VER,
                version);
        return ADB_ERR_UNSUPPORTED;
    }

    type = buffer[type_offset];
    if(type >= ADB__PAIRING_COUNT)
    {
        ADB__ERROR("unsupported pairing header type");
        ADB__INFO("receieved header type: 0x%02X", type);
        return ADB_ERR_UNSUPPORTED;
    }

    header->type = type;
    header->version = version;
    header->payload_size = 
        (uint32_t)(buffer[plsz_offset] << 24) |
        (uint32_t)(buffer[plsz_offset + 1] << 16) |
        (uint32_t)(buffer[plsz_offset + 2] << 8) |
        (uint32_t)buffer[plsz_offset + 3];

    ADB__INFO("read pairing header sucessfully");
    return ADB_ERR_OK;
}

static adb_error_t adb__write_pairing_header(
        adb_conn_t *conn,
        adb__pairing_header_t *header)
{
    static const size_t version_offset =
        offsetof(adb__pairing_header_t, version);
    static const size_t type_offset =
        offsetof(adb__pairing_header_t, type);
    static const size_t plsz_offset = 
        offsetof(adb__pairing_header_t, payload_size);
    uint8_t buffer[sizeof(*header)] = {0};
    adb_error_t res = ADB_ERR_OK;

    if(!conn || !header)
        return ADB_ERR_PARAM;

    ADB__INFO("writing pairing header");

    buffer[version_offset] = header->version;
    buffer[type_offset] = header->type;
    buffer[plsz_offset]     = (header->payload_size & 0xFF000000) >> 24;
    buffer[plsz_offset + 1] = (header->payload_size & 0x00FF0000) >> 16;
    buffer[plsz_offset + 2] = (header->payload_size & 0x0000FF00) >> 8;
    buffer[plsz_offset + 3] = (header->payload_size & 0x000000FF);

    /*
    adb__log_print_payload(
            buffer, sizeof(buffer), 
            "serialized sent pairing header");
    */

    res = adb__conn_write(conn, buffer, sizeof(buffer));
    if(res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to write pairing header");
        return res;
    }

    ADB__INFO("wrote pairing header sucessfully");
    return ADB_ERR_OK;
}

adb_error_t adb_conn_pair(
        adb_conn_t *conn,
        const char *code,
        const size_t code_len,
        adb_key_t *key)
{
    static const uint8_t client_name[] = "adb pair client";
    static const uint8_t server_name[] = "adb pair server";
    adb_error_t ret = ADB_ERR_OK;
    uint8_t keying_material[ADB__TLS_EXPORTED_KEY_SIZE] = {0};
    uint8_t private_key[4096] = {0};
    uint8_t x509_cert[4096] = {0};

    spake2_ctx_t *spake2 = NULL;
    uint8_t random_data[SPAKE2_RANDOM_DATA_LENGTH] = {0};
    uint8_t password[ADB__PAIRING_CODE_DIGITS + sizeof(keying_material)] = {0};
    uint8_t spake2_msg[SPAKE2_MAX_MESSAGE_LENGTH] = {0};
    size_t msg_size = 0;
    
    adb__pairing_header_t header = {0};
    uint8_t *pairing_payload = NULL;
    if(!conn || !adb__verify_pairing_code(code, code_len))
        return ADB_ERR_PARAM;
    if(!conn->tls.initialized)
        return ADB_ERR_UNSUPPORTED;

    ADB__INFO("pairing wireless device with code \"%6s\"", code);

    
    ret = adb__tls_handshake(&conn->tls);
    if(ret != ADB_ERR_OK)
        return ret;

    ret = adb__tls_export_keying_material(
            &conn->tls,
            keying_material);
    if(ret != ADB_ERR_OK)
        return ret;

    adb__tls_export_keying_material(&conn->tls, keying_material);
    adb__log_print_payload(
            keying_material, 
            ADB__TLS_EXPORTED_KEY_SIZE, 
            "wireless device keying material");

    if(
            !adb__key_write_x509_pem(key, conn->ctx, 
                x509_cert, sizeof(x509_cert)) ||
            !adb__key_write_pkcs8_pem(key, 
                private_key, sizeof(private_key)))
    {
        return ADB_ERR_CRYPTO;
    }
    
    mbedtls_ctr_drbg_random(
            &conn->ctx->drbg,
            random_data,
            sizeof(random_data));

    memcpy(password, code, ADB__PAIRING_CODE_DIGITS);
    memcpy(password + ADB__PAIRING_CODE_DIGITS, 
            keying_material, sizeof(keying_material));
    adb__log_print_payload(
            password, 
            sizeof(password), 
            "spake2 password");

    spake2 = spake2_ctx_new(
            &(spake2_allocator_t) {
                .malloc = adb__alloc_get()->malloc,
                .free = adb__alloc_get()->free,
                .userdata = adb__alloc_get()->userdata
            }, SPAKE2_ROLE_ALICE,
            client_name, sizeof(client_name),
            server_name, sizeof(server_name));

    spake2_generate_msg(
            spake2, 
            spake2_msg, 
            &msg_size, 
            sizeof(spake2_msg),
            password, 
            sizeof(password),
            random_data);

    adb__log_print_payload(
            spake2_msg, msg_size, 
            "spake2 message"); 

    header.version = ADB__PAIRING_HEADER_VER;
    header.type = ADB__PAIRING_SPAKE2_MSG;
    header.payload_size = (uint32_t)msg_size;

    ret = adb__write_pairing_header(conn, &header);
    if(ret != ADB_ERR_OK)
        goto cleanup;

    ret = adb__conn_write(conn, spake2_msg, msg_size);
    if(ret != ADB_ERR_OK)
    {
        adb__log_err_adb(ret, "failed to send spake2 message");
        goto cleanup;
    }

    ret = adb__read_pairing_header(conn, &header);
    if(ret != ADB_ERR_OK)
        goto cleanup;

    if(header.type != ADB__PAIRING_SPAKE2_MSG)
    {
        ADB__ERROR("unexpected pairing header type");
        ADB__INFO("expected: 0x%02X, got: 0x%02X", 
                (unsigned)ADB__PAIRING_SPAKE2_MSG, header.type);
        ret = ADB_ERR_PROTOCOL;
        goto cleanup;
    }
    
    pairing_payload = adb__malloc(header.payload_size);
    if(!pairing_payload)
        { ret = ADB_ERR_NO_MEM; goto cleanup; }

    ret = adb__conn_read(conn, 
            pairing_payload, header.payload_size);
    if(ret != ADB_ERR_OK)
    {
        adb__log_err_adb(ret, "failed to read pairing payload");
        goto cleanup;
    }
    adb__log_print_payload(
            pairing_payload, header.payload_size, 
            "response pairing payload");
    
    ADB__INFO("pairing wireless device successfully");

cleanup:
    spake2_ctx_free(spake2);
    adb__free(pairing_payload);
    return ret;
}

#define ADB__MAX_SUPPORTED_VER 0x01000001
#define ADB__MAX_ADB_PAYLOAD_SIZE (1024 * 1024)

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
    pkt.msg.arg1 = ADB__MAX_ADB_PAYLOAD_SIZE;
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
    
    adb__log_print_payload(buf, size, "read data");
    return res;
}


adb_error_t adb__conn_write(
        adb_conn_t *conn,
        const void *buf,
        const size_t size)
{
    if(!conn)
        return ADB_ERR_PARAM;

    adb__log_print_payload(buf, size, "write data");

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
