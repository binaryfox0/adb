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
#include "adb_decomp.h"

#include "adb_transport_usb.h"
#include "adb_transport_tcp.h"
#include "adb_transport_custom.h"

#define ADB__PATH_MAX                   4096
#define ADB__FEATURE_SENDRECV_V2        "sendrecv_v2"
#define ADB__FEATURE_SENDRECV_V2_BROTLI "sendrecv_v2_brotli"
#define ADB__FEATURE_SENDRECV_V2_LZ4    "sendrecv_v2_lz4"
#define ADB__FEATURE_SENDRECV_V2_ZSTD   "sendrecv_v2_zstd"
#define ADB__FEATURE_DELAYED_ACK        "delayed_ack"

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

static bool adb__has_feature(
        adb_conn_t *conn,
        const char *feature)
{
    adb__str_t cur = {0};
    adb__str_t token = {0};
    if(!conn || !feature)
        return false;

    cur = conn->features;
    while(adb__str_next_tok(&cur, ',', &token))
    {
        if(adb__str_compare_cstr(&token, feature))
            return true;
    }
    return false;
}

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
        const adb_read_fn read_cb,
        const adb_write_fn write_cb,
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

adb_error_t adb_handshake(
        adb_conn_t *conn, 
        adb_key_t *key)
{
    adb_error_t ret = ADB_ERR_OK;
    static const char conn_str[] = 
        "host::features="
        ADB__FEATURE_SENDRECV_V2 ","
        ADB__FEATURE_DELAYED_ACK;
    adb__packet_t pkt = {0};
    uint8_t *payload = NULL;
    
    if (!conn || !key)
        return ADB_ERR_PARAM;
    
    pkt.command = ADB__CMD_CNXN;
    pkt.arg0 = ADB__PACKET_MAX_SUPPORTED_VER;
    pkt.arg1 = ADB__PACKET_MAX_PAYLOAD_SIZE;
    pkt.payload_size = sizeof(conn_str) - 1;


    if(
            (ret = adb__packet_write(conn, &pkt, 
                                     conn_str)) != ADB_ERR_OK ||
            (ret = adb__packet_read(conn, &pkt, 
                                    (void **)&payload) != ADB_ERR_OK))
        return ret;

    if (pkt.command == ADB__CMD_STLS) 
    {
        pkt.command = ADB__CMD_STLS;
        pkt.arg0 = ADB__STLS_VERSION;
        pkt.arg1 = 0;
        pkt.payload_size = 0;
    
        ret = adb__packet_write(conn, &pkt, NULL);
        if (ret != ADB_ERR_OK)
            goto fail;

        ret = adb__conn_upgrade_tls(conn, key);
        if (ret != ADB_ERR_OK)
            goto fail;

        adb__free(payload); payload = NULL;
        ret = adb__packet_read(conn, &pkt, (void **)&payload);
        if (ret != ADB_ERR_OK)
            goto fail;
    } else if(pkt.command == ADB__CMD_AUTH) {
        /*
         * A valid adb_key_t always has a valid private key object,
         * so the implementation for ADB__AUTH_PUBLIC_KEY will
         * always be useless.
         */
        uint8_t signature[ADB__KEY_SIGNATURE_LENGTH] = {0};
        pkt.command = ADB__CMD_AUTH;
        pkt.arg0 = ADB__AUTH_SIGNATURE;
        pkt.arg1 = 0;
        ret = adb__key_sign(key, conn->ctx, 
                (const char*)payload, pkt.payload_size, 
                signature);
        if(ret != ADB_ERR_OK)
            goto fail;
        pkt.payload_size = sizeof(signature);
       
        ret = adb__packet_write(conn, &pkt, signature);
        if (ret != ADB_ERR_OK)
            goto fail;
        
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

#define ADB__SYNC_ID_RECV_V2 ADB__CMD_ENCODE('R', 'C', 'V', '2')
#define ADB__SYNC_ID_DONE ADB__CMD_ENCODE('D', 'O', 'N', 'E')
#define ADB__SYNC_ID_DATA ADB__CMD_ENCODE('D', 'A', 'T', 'A')

#define ADB__SYNC_DATA_MAX (64 * 1024)
#define ADB__INIT_DELAYED_ACK_BYTES (32 * 1024 * 1024)

typedef struct __attribute__((packed)) 
{
    uint32_t id;
    uint32_t path_len;
    /* followed by `path_length` bytes of non-null terminated path */
} adb__sync_request_t;

typedef struct __attribute__((packed)) 
{
    uint32_t id;
    uint32_t flags;
} adb__sync_recv_v2_t;

typedef struct __attribute__((packed))
{
    uint32_t id;
    uint32_t size;
    /* followed by `size` bytes of data. */
} adb__sync_data_t;

typedef struct __attribute__((packed))
{
    uint32_t id;
    uint32_t error;
    uint64_t dev;
    uint64_t ino;
    uint32_t mode;
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    int64_t atime;
    int64_t mtime;
    int64_t ctime;
    uint32_t namelen;
    /* followed by `namelen` bytes of name */
} adb__sync_dent_v2_t;

typedef enum 
{
    ADB__SYNC_FLAG_NONE = 0,
    ADB__SYNC_FLAG_BROTLI = (1 << 0),
    ADB__SYNC_FLAG_LZ4 = (1 << 1),
    ADB__SYNC_FLAG_ZSTD = (1 << 2)
} adb__sync_flag_t;

/* docs later: sync data header and payload can be spread across WRTN */
static adb_error_t adb__pull_impl(
        adb_conn_t *conn,
        const char *path,
        const adb__decomp_type_t decomp_type,
        const adb_write_fn write_fn,
        void *userdata)
{
    static uint32_t decomp_type_to_flags[ADB__DECOMP_COUNT] =
    {
        [ADB__DECOMP_NONE] = 0,
        [ADB__DECOMP_BROTLI] = ADB__SYNC_FLAG_BROTLI,
        [ADB__DECOMP_LZ4] = ADB__SYNC_FLAG_LZ4,
        [ADB__DECOMP_ZSTD] = ADB__SYNC_FLAG_ZSTD,
    };

    adb_error_t ret = ADB_ERR_OK;
    adb_error_t ret2 = ADB_ERR_OK;

    size_t path_len = 0;
    size_t req_size = 0;

    uint32_t local_id = 0;
    uint32_t remote_id = 0;

    bool delayed_ack = false;
    bool data_done = false;
    bool ack_received = false;

    adb__packet_t pkt = {0};
    uint8_t *pkt_payload = NULL;
    int32_t initial_asb = 0;
    int32_t asb = 0;
    adb__sync_request_t req = {0};
    adb__sync_recv_v2_t msg = {0};

    uint8_t req_payload[
            sizeof(req) +
            ADB__PATH_MAX +
            sizeof(msg)] = {0};
    uint8_t *req_cursor = NULL;

    adb__decomp_t *decomp = NULL;
    /* vars to track header and data across WRTE */
    size_t data_remaining = 0;
    size_t header_written = 0;
    adb__sync_data_t header = {0};

    ADB__INFO("pulling file from device at \"%s\"", path);

    path_len = strlen(path);
    if(path_len > ADB__PATH_MAX)
        return ADB_ERR_PARAM;

    req_size = sizeof(req) + path_len + sizeof(msg);
    local_id = 1;
    delayed_ack = adb__has_feature(conn, ADB__FEATURE_DELAYED_ACK);

    /*
     * OPEN(local-id, [send-buffer], "destination")
     * The send-buffer value advertises delayed-ACK support.
     */
    pkt.command = ADB__CMD_OPEN;
    pkt.arg0 = local_id;
    pkt.arg1 = delayed_ack ? ADB__INIT_DELAYED_ACK_BYTES : 0;

    /* null byte for compatibility */
    pkt.payload_size = sizeof("sync:");

    ret = adb__packet_write(conn, &pkt, "sync:");
    if(ret != ADB_ERR_OK)
    {
        adb__log_err_adb(ret, "failed to open stream");
        return ret;
    }

    /*
     * The first OKAY establishes the remote stream ID.
     * With delayed ACK enabled, its payload contains the initial ASB.
     */
    ret = adb__packet_read_into(
            conn,
            &pkt,
            &initial_asb,
            delayed_ack ? sizeof(initial_asb) : 0);
    if(ret != ADB_ERR_OK)
        return ret;

    if(!adb__packet_check_cmd(&pkt, ADB__CMD_OKAY))
    {
        ret = ADB_ERR_PROTOCOL;
        goto cleanup;
    }

    remote_id = pkt.arg0;

    req.id = ADB__SYNC_ID_RECV_V2;
    req.path_len = (uint32_t)path_len;
    msg.id = ADB__SYNC_ID_RECV_V2;
    msg.flags = decomp_type_to_flags[decomp_type];

    req_cursor = req_payload;
    req_cursor = adb__mempcpy(req_cursor, &req, sizeof(req));
    req_cursor = adb__mempcpy(req_cursor, path, path_len);
    memcpy(req_cursor, &msg, sizeof(msg));

    pkt.command = ADB__CMD_WRTE;
    pkt.arg0 = local_id;
    pkt.arg1 = remote_id;
    pkt.payload_size = (uint32_t)req_size;

    ret = adb__packet_write(conn, &pkt, req_payload);
    if(ret != ADB_ERR_OK)
        goto cleanup;
   
    ret = adb__decomp_create(&decomp, decomp_type);
    if(ret != ADB_ERR_OK)
        goto cleanup;

    while(!data_done || !ack_received)
    {
        size_t pkt_offset = 0;

        adb__free(pkt_payload);
        pkt_payload = NULL;

        ret = adb__packet_read(
                conn,
                &pkt,
                (void**)&pkt_payload);
        if(ret != ADB_ERR_OK)
            goto cleanup;

        if(pkt.command != ADB__CMD_OKAY &&
                pkt.command != ADB__CMD_WRTE)
        {
            ret = ADB_ERR_PROTOCOL;
            goto cleanup;
        }

        if(pkt.arg0 != remote_id || pkt.arg1 != local_id)
        {
            ADB__ERROR("unexpected packet IDs");
            ADB__INFO(
                    "expected local: %u, remote: %u",
                    local_id,
                    remote_id);
            ADB__INFO(
                    "got local: %u, remote: %u",
                    pkt.arg1,
                    pkt.arg0);

            ret = ADB_ERR_PROTOCOL;
            goto cleanup;
        }

        if(pkt.command == ADB__CMD_OKAY)
        {
            if(delayed_ack)
            {
                if(!adb__packet_check_size(&pkt, sizeof(initial_asb)))
                {
                    ret = ADB_ERR_PROTOCOL;
                    goto cleanup;
                }
                memcpy(&initial_asb, pkt_payload, sizeof(initial_asb));
            }
            else if(!adb__packet_check_size(&pkt, 0))
            {
                ret = ADB_ERR_PROTOCOL;
                goto cleanup;
            }

            ack_received = true;
            continue;
        }

        while(pkt_offset < pkt.payload_size && !data_done)
        {
            size_t pkt_remaining = 0;
            size_t available = 0;
            size_t header_remaining = 0;

            pkt_remaining = pkt.payload_size - pkt_offset;

            /* The data may be split across multiple WRTE packets. */
            if(data_remaining > 0)
            {
                available = ADB__MIN(pkt_remaining, data_remaining);
                ret = adb__decomp_decompress(decomp, 
                        pkt_payload + pkt_offset, 
                        available, write_fn, userdata);
                if(ret != ADB_ERR_OK)
                    goto cleanup;

                pkt_offset += available;
                data_remaining -= available;
                continue;
            }

            /* The header may be split across multiple WRTE packets. */
            header_remaining = sizeof(header) - header_written;
            available = ADB__MIN(pkt_remaining, header_remaining);

            memcpy(
                    (uint8_t*)&header + header_written,
                    pkt_payload + pkt_offset,
                    available);

            pkt_offset += available;
            header_written += available;

            if(header_written != sizeof(header))
                continue;

            header_written = 0;
            if(header.id == ADB__SYNC_ID_DONE)
            {
                if(header.size != 0)
                {
                    ADB__ERROR("unexpected data payload");
                    ADB__INFO(
                            "expected: 0 bytes, got: %u bytes",
                            header.size);

                    ret = ADB_ERR_PROTOCOL;
                    goto cleanup;
                }

                data_done = true;
                break;
            }

            if(header.id != ADB__SYNC_ID_DATA)
            {
                ADB__ERROR("unexpected data id");
                ADB__INFO(
                        "expected: 0x%08X (%.4s), got: 0x%08X (%.4s)",
                        (uint32_t)ADB__SYNC_ID_DATA,
                        (uint8_t*)(uint32_t[1]){ADB__SYNC_ID_DATA},
                        header.id,
                        (uint8_t*)&header.id);

                ret = ADB_ERR_PROTOCOL;
                goto cleanup;
            }

            if(header.size > ADB__SYNC_DATA_MAX)
            {
                ADB__ERROR("data size exceeds the maximum allowed size");
                ADB__INFO(
                        "max size: %d bytes, got: %u bytes",
                        ADB__SYNC_DATA_MAX,
                        header.size);

                ret = ADB_ERR_PROTOCOL;
                goto cleanup;
            }

            data_remaining = header.size;
        }

        /*
         * Account for the complete WRTE packet before sending its ACK.
         */
        asb -= (int32_t)pkt.payload_size;

        pkt.command = ADB__CMD_OKAY;
        pkt.arg0 = local_id;
        pkt.arg1 = remote_id;
        pkt.payload_size = 0;

        if(delayed_ack && asb < 0)
        {
            int32_t consumed = initial_asb - asb;
            pkt.payload_size = sizeof(consumed);
            ret = adb__packet_write(
                    conn,
                    &pkt,
                    &consumed);
        }
        else
        {
            ret = adb__packet_write(
                    conn,
                    &pkt,
                    NULL);
        }

        if(ret != ADB_ERR_OK)
        {
            adb__log_err_adb(
                    ret,
                    "failed to send acknowledgement signal");
            goto cleanup;
        }
    }

    ADB__INFO("pulled file from device successfully");

cleanup:
    pkt.command = ADB__CMD_CLSE;
    pkt.arg0 = local_id;
    pkt.arg1 = remote_id;
    pkt.payload_size = 0;

    ret2 = adb__packet_write(conn, &pkt, NULL);
    if(ret2 != ADB_ERR_OK)
    {
        adb__log_err_adb(ret2, "failed to close stream");
        ret = ret != ADB_ERR_OK ? ret : ret2;
    }

    adb__decomp_destroy(decomp);
    adb__free(pkt_payload);
    return ret;
}

adb_error_t adb_pull(
        adb_conn_t *conn,
        const char *path,
        const adb_write_fn write_fn,
        void *userdata)
{
    struct {
        const char *feature;
        adb__decomp_type_t decomp_type;
    } check_orders[ADB__DECOMP_COUNT] =
    {
        { ADB__FEATURE_SENDRECV_V2_BROTLI, ADB__DECOMP_BROTLI },
        { ADB__FEATURE_SENDRECV_V2_LZ4, ADB__DECOMP_LZ4 },
        { ADB__FEATURE_SENDRECV_V2_ZSTD, ADB__DECOMP_ZSTD },
        { ADB__FEATURE_SENDRECV_V2, ADB__DECOMP_NONE },
    };

    if(!conn || !path || !write_fn)
        return ADB_ERR_PARAM;

    for(size_t i = 0; i < ADB__ARRSZ(check_orders); i++)
    {
        /* XXX: dunno it needs to try another if current failed */
        if(adb__has_feature(conn, check_orders[i].feature))
        {
            return adb__pull_impl(conn, path, 
                    check_orders[i].decomp_type, 
                    write_fn, userdata);
        }
    }
    return ADB_ERR_UNSUPPORTED;
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

adb_error_t adb__conn_read_alloc(
        adb_conn_t *conn,
        void **out,
        const size_t size)
{
    adb_error_t ret = ADB_ERR_OK;
    uint8_t *buf = NULL;

    buf = adb__malloc(size);
    if(!buf)
        return ADB_ERR_NO_MEM;

    ret = adb__conn_read(conn, buf, size);
    if(ret != ADB_ERR_OK)
        adb__free(buf);
    else
        *out = buf;

    return ret;
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

