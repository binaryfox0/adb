#include <adb/adb_pair.h>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>

#include <spake2.h>
#include <mbedtls/hkdf.h>

#include <adb/adb_conn.h>
#include "adb_conn_priv.h"
#include <adb/adb_key.h>
#include "adb_key_priv.h"
#include "adb_utils.h"
#include "adb_log_priv.h"
#include "adb_tls.h"
#include "adb_ctx_priv.h"
#include "adb_alloc_priv.h"
#include "adb_aes.h"

#define ADB__PAIR_CODE_DIGITS       6
#define ADB__PAIR_HEADER_VER        1
#define ADB__PAIR_HEADER_MIN_VER    1
#define ADB__PAIR_HEADER_MAX_VER    1
#define ADB__MAX_PEER_INFO_SIZE     8192
#define ADB__PAIR_MAX_PAYLOAD_SIZE  (ADB__MAX_PEER_INFO_SIZE * 2)
#define ADB__SPAKE2_PASSWORD_LENGTH \
    (ADB__PAIR_CODE_DIGITS + ADB__TLS_EXPORTED_KEY_LENGTH)
#define ADB__QR_RANDOM_LENGTH 10

typedef enum 
{
    ADB__PAIR_PACKET_SPAKE2,
    ADB__PAIR_PACKET_PEER_INFO,
    ADB__PAIR_PACKET_COUNT,
} adb__pair_packet_type_t;

typedef struct __attribute__((packed))
{
    uint8_t version;   // PairingPacket version
    uint8_t type;      // the type of packet (PairingPacket.Type)
    uint32_t size;     // Size of the payload in bytes
} adb__pair_packet_t;

typedef enum 
{
    ADB__PEER_INFO_PUBLIC_KEY,
    ADB__PEER_INFO_DEVICE_GUID,
    ADB__PEER_INFO_COUNT
} adb__peer_info_type_t;

typedef struct __attribute__((packed))
{
    uint8_t type;
    uint8_t data[ADB__MAX_PEER_INFO_SIZE - 1];
} adb__peer_info_t;

static const char *adb__pair_packet_type_strings[ADB__PAIR_PACKET_COUNT] =
{
    ADB__ENUM_KEY_VALUE(ADB__PAIR_PACKET_SPAKE2),
    ADB__ENUM_KEY_VALUE(ADB__PAIR_PACKET_PEER_INFO)
};

static const char *adb__peer_info_type_strings[ADB__PAIR_PACKET_COUNT] =
{
    ADB__ENUM_KEY_VALUE(ADB__PEER_INFO_PUBLIC_KEY),
    ADB__ENUM_KEY_VALUE(ADB__PEER_INFO_DEVICE_GUID)
};

static inline bool adb__verify_pairing_code(
        const char *code)
{
    if(!code && strlen(code) != ADB__PAIR_CODE_DIGITS)
        return false;
    for(size_t i = 0; i < ADB__PAIR_CODE_DIGITS; i++)
    {
        if(!isdigit(code[i]))
            return false;
    }
    return true;
}

static void adb__log_pair_packet(
        const adb__pair_packet_t *pkt)
{
    ADB__DEBUG("ppkt: version=0x%02X, type=0x%02X (%s), size=0x%08X bytes",
            pkt->version, 
            pkt->type, adb__pair_packet_type_strings[pkt->type],
            pkt->size);
}

static adb_error_t adb__pair_read_packet(
        adb_conn_t *conn,
        adb__pair_packet_t *out_pkt,
        void *out_payload,
        const size_t size)
{
    static const size_t version_offset =
        offsetof(adb__pair_packet_t, version);
    static const size_t type_offset =
        offsetof(adb__pair_packet_t, type);
    static const size_t plsz_offset = 
        offsetof(adb__pair_packet_t, size);
    uint8_t buffer[sizeof(*out_pkt)] = {0};
    adb_error_t res = ADB_ERR_OK;
    uint8_t version = 0;
    uint8_t type = 0;
    uint32_t dec_size = 0;

    if(!conn || !out_pkt)
        return ADB_ERR_PARAM;

    ADB__INFO("reading pairing packet");

    res = adb__conn_read(conn, buffer, sizeof(buffer));
    if(res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to read pairing header");
        return res;
    }

    version = buffer[version_offset];
    if(!ADB__IN_RANGE(version, 
                ADB__PAIR_HEADER_MIN_VER, 
                ADB__PAIR_HEADER_MAX_VER))
    {
        ADB__ERROR("unsupported pairing header version");
        ADB__INFO("supported version range %d,%d, got: %d",
                ADB__PAIR_HEADER_MIN_VER,
                ADB__PAIR_HEADER_MAX_VER,
                version);
        return ADB_ERR_UNSUPPORTED;
    }

    type = buffer[type_offset];
    if(type >= ADB__PAIR_PACKET_COUNT)
    {
        ADB__ERROR("unsupported pairing header type");
        ADB__INFO("receieved pairing packet type: 0x%02X", type);
        return ADB_ERR_UNSUPPORTED;
    }

    dec_size = 
        (uint32_t)(buffer[plsz_offset] << 24) |
        (uint32_t)(buffer[plsz_offset + 1] << 16) |
        (uint32_t)(buffer[plsz_offset + 2] << 8) |
        (uint32_t)buffer[plsz_offset + 3];

    if(!ADB__IN_RANGE(dec_size, 1, ADB__PAIR_MAX_PAYLOAD_SIZE))
    {
        ADB__ERROR("pairing payload size not within a safe range");
        ADB__INFO("safe range: [%d, %d], got: %u bytes",
                1, ADB__PAIR_MAX_PAYLOAD_SIZE, dec_size);
        return ADB_ERR_PROTOCOL;
    }

    if(dec_size > size)
    {
        ADB__ERROR("pairing payload size is not within requested size");
        ADB__INFO("max size: %zu bytes, got: %u bytes", size, dec_size);
        return ADB_ERR_PROTOCOL;
    }

    res = adb__conn_read(conn, out_payload, dec_size);
    if(res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to read pairing payload");
        return res;
    }

    out_pkt->version = version;
    out_pkt->type = type;
    out_pkt->size = dec_size;

    ADB__INFO("read pairing packet sucessfully");
    adb__log_pair_packet(out_pkt);
    return ADB_ERR_OK;
}

static adb_error_t adb__pair_write_packet(
        adb_conn_t *conn,
        adb__pair_packet_t *pkt,
        const void *payload)
{
    static const size_t version_offset =
        offsetof(adb__pair_packet_t, version);
    static const size_t type_offset =
        offsetof(adb__pair_packet_t, type);
    static const size_t plsz_offset = 
        offsetof(adb__pair_packet_t, size);
    uint8_t buffer[sizeof(*pkt)] = {0};
    adb_error_t res = ADB_ERR_OK;

    if(!conn || !pkt)
        return ADB_ERR_PARAM;

    ADB__INFO("writing pairing packet");
    adb__log_pair_packet(pkt);

    buffer[version_offset] = pkt->version;
    buffer[type_offset] = pkt->type;
    buffer[plsz_offset]     = (pkt->size & 0xFF000000) >> 24;
    buffer[plsz_offset + 1] = (pkt->size & 0x00FF0000) >> 16;
    buffer[plsz_offset + 2] = (pkt->size & 0x0000FF00) >> 8;
    buffer[plsz_offset + 3] = (pkt->size & 0x000000FF);

    res = adb__conn_write(conn, buffer, sizeof(buffer));
    if(res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to write pairing header");
        return res;
    }

    res = adb__conn_write(conn, payload, pkt->size);
    if(res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to write pairing payload");
        return res;
    }

    ADB__INFO("wrote pairing packet sucessfully");
    return ADB_ERR_OK;
}

static inline bool adb__pair_check_packet(
        adb__pair_packet_t *pkt,
        const adb__pair_packet_type_t type)
{
    if(!pkt)
        return false;

    if(pkt->type != type)
    {
        ADB__ERROR("unexpected pairing header type");
        ADB__INFO("expected: 0x%02X (%s), got: 0x%02X (%s)", 
                type, adb__pair_packet_type_strings[type],
                pkt->type, adb__pair_packet_type_strings[pkt->type]);
        return false;
    }
    
    return true;
}

static adb_error_t adb__exchange_message(
        adb_conn_t *conn,
        const uint8_t *password,
        const size_t password_len,
        uint8_t out_key_material[SPAKE2_MAX_KEY_LENGTH],
        size_t *out_key_material_len)
{
    static const uint8_t client_name[] = "adb pair client";
    static const uint8_t server_name[] = "adb pair server";

    adb_error_t ret = ADB_ERR_OK;

    int err = 0;
    uint8_t random_data[SPAKE2_RANDOM_DATA_LENGTH] = {0};
    spake2_ctx_t *spake2 = NULL;
    uint8_t my_msg[SPAKE2_MAX_MESSAGE_LENGTH] = {0};
    size_t my_msg_size = 0;
    
    adb__pair_packet_t pkt = {0};
    uint8_t their_msg[SPAKE2_MAX_MESSAGE_LENGTH] = {0};

    ADB__INFO("exchanging SPAKE2 message with the device");

    
    err = mbedtls_ctr_drbg_random(
            &adb__conn_get_ctx(conn)->drbg,
            random_data,
            sizeof(random_data));
    if(err != 0)
    {
        adb__log_err_mbedtls(err, 
                "failed to generate random data to generate message");
        goto cleanup;
    }

    adb__log_payload(password, password_len, "spake2 password");
    spake2 = spake2_ctx_create(
            &(spake2_allocator_t) {
                .malloc = adb__alloc_get()->malloc,
                .free = adb__alloc_get()->free,
                .userdata = adb__alloc_get()->userdata
            }, SPAKE2_ROLE_ALICE,
            client_name, sizeof(client_name),
            server_name, sizeof(server_name));
    if(!spake2)
    {
        ADB__ERROR("failed to create SPAKE2 context");
        ADB__INFO("reason: out of memory");
        return ADB_ERR_NO_MEM;
    }

    if(!spake2_generate_msg(
            spake2, 
            my_msg, 
            &my_msg_size, 
            sizeof(my_msg),
            password, 
            password_len,
            random_data))
    {
        ADB__ERROR("failed to generate SPAKE2 message");
        goto cleanup;
    }

    pkt.version = ADB__PAIR_HEADER_VER;
    pkt.type = ADB__PAIR_PACKET_SPAKE2;
    pkt.size = (uint32_t)my_msg_size;

    ret = adb__pair_write_packet(conn, &pkt, my_msg);
    if(ret != ADB_ERR_OK)
        goto cleanup;

    ret = adb__pair_read_packet(conn, &pkt, 
            their_msg, sizeof(their_msg));
    if(ret != ADB_ERR_OK)
        goto cleanup;
    if(!adb__pair_check_packet(&pkt, ADB__PAIR_PACKET_SPAKE2))
    {
        ret = ADB_ERR_PROTOCOL;
        goto cleanup;
    }

    if(!spake2_process_msg(
                spake2, 
                out_key_material, 
                out_key_material_len, 
                SPAKE2_MAX_KEY_LENGTH,
                their_msg, 
                pkt.size))
    {
        ADB__ERROR("failed to process their public key");
        ret = ADB_ERR_CRYPTO; // XXX: should be another err since
                              // it came from the device
        goto cleanup;
    }
    
    ADB__INFO("exchanged SPAKE2 message with the device successfully");

cleanup:
    spake2_ctx_free(spake2);
    return ret;
}

static adb_error_t adb__exchange_info(
        adb_conn_t *conn,
        const uint8_t *key_material,
        const size_t key_material_len,
        adb_key_t *key,
        char out_guid[ADB__MEMSZ(adb__peer_info_t, data)])
{
    adb_error_t ret = ADB_ERR_OK;
    adb__aes_t aes = {0};
    adb__peer_info_t my_info = {0};
    uint8_t enc_my_info[ADB__AES_ENCYPTED_SIZE(sizeof(my_info))] = {0};
    adb__pair_packet_t pkt = {0};
    
    uint8_t enc_their_info[ADB__AES_ENCYPTED_SIZE(sizeof(my_info))] = {0};
    adb__peer_info_t their_info = {0};
    bool has_null = false;

    ADB__INFO("exchanging info with device");

    ret = adb__aes_init(&aes, key_material, key_material_len);
    if(ret != ADB_ERR_OK)
        goto cleanup;

    my_info.type = ADB__PEER_INFO_PUBLIC_KEY; 
    ret = adb_key_generate_pubkey(
            key, 
            my_info.data, 
            sizeof(my_info.data), 
            NULL);
    if(ret != ADB_ERR_OK)
        goto cleanup;

    adb__log_multiline_text((const char*)my_info.data, "public key");

    ret = adb__aes_encrypt(
            &aes,
            &my_info,
            sizeof(my_info),
            enc_my_info,
            sizeof(enc_my_info));
    if(ret != ADB_ERR_OK)
        goto cleanup;

    pkt.version = ADB__PAIR_HEADER_VER;
    pkt.type = ADB__PAIR_PACKET_PEER_INFO;
    pkt.size = (uint32_t)sizeof(enc_my_info);

    ret = adb__pair_write_packet(conn, &pkt, enc_my_info);
    if(ret != ADB_ERR_OK)
        goto cleanup; 
   
    ret = adb__pair_read_packet(conn, &pkt, 
            enc_their_info, sizeof(enc_their_info));
    if(ret != ADB_ERR_OK)
        goto cleanup; 
    if(!adb__pair_check_packet(&pkt, ADB__PAIR_PACKET_PEER_INFO))
    {
        ret = ADB_ERR_PROTOCOL;
        goto cleanup;
    }

    ret = adb__aes_decrypt(
            &aes,
            enc_their_info,
            pkt.size,
            &their_info,
            sizeof(their_info));
    if(ret != ADB_ERR_OK)
        goto cleanup;

    adb__log_payload(
            &their_info, sizeof(their_info), 
            "their info");
    if(their_info.type >= ADB__PEER_INFO_COUNT)
    {
        ADB__ERROR("unsupported peer info type");
        ADB__INFO("recieved peer info type: 0x%02X", their_info.type);
        ret = ADB_ERR_UNSUPPORTED;
        goto cleanup;
    }
    if(their_info.type != ADB__PEER_INFO_DEVICE_GUID)
    {
        ADB__ERROR("unexpected peer info type");
        ADB__INFO("expected: 0x%02X (%s), got: 0x%02X (%s)",
                (uint32_t)ADB__PEER_INFO_DEVICE_GUID,
                ADB__STRINGIFY(ADB__PEER_INFO_DEVICE_GUID),
                their_info.type,
                adb__peer_info_type_strings[their_info.type]);
        ret = ADB_ERR_PROTOCOL;
        goto cleanup;
    }

    for(size_t i = 0; i < sizeof(their_info.data); i++)
    {
        if(their_info.data[i] == '\0')
        {
            has_null = true;
            break;
        }
    }

    if(!has_null)
    {
        ADB__ERROR("corrupted peer info was recieved");
        ADB__INFO("reason: missing null byte for the data");
        ret = ADB_ERR_PROTOCOL;
        goto cleanup;
    }

    memcpy(out_guid, their_info.data, sizeof(their_info.data));
    ADB__INFO("exchanged info with the device guid=\"%s\" successfully",
            out_guid);

cleanup:
    adb__aes_destroy(&aes);
    return ret;
}

adb_error_t adb_pair(
        adb_conn_t *conn,
        const char *code,
        adb_key_t *key,
        char *out_guid,
        const size_t out_guid_len)
{
    adb_error_t res = ADB_ERR_OK;

    uint8_t password[ADB__PAIR_CODE_DIGITS + ADB__TLS_EXPORTED_KEY_LENGTH] = {0};
    void *p = NULL;

    uint8_t key_material[SPAKE2_MAX_KEY_LENGTH] = {0};
    size_t key_material_len = 0;
    char device_guid[ADB__MEMSZ(adb__peer_info_t, data)] = {0};
    size_t guid_len = 0;

    if(!conn || !adb__verify_pairing_code(code) || 
            (out_guid == 0 ^ out_guid_len == 0))
        return ADB_ERR_PARAM;

    ADB__INFO("pairing wireless device with code \"%6s\"", code);
    res = adb__conn_upgrade_tls(conn, key);
    if(res != ADB_ERR_OK)
        return res;
    
    p = adb__mempcpy(password, code, ADB__PAIR_CODE_DIGITS);
    res = adb__tls_export_keying_material(
            adb__conn_get_tls(conn), p);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__exchange_message(
            conn, 
            password, sizeof(password), 
            key_material, &key_material_len);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__exchange_info(
            conn, 
            key_material,
            key_material_len,
            key,
            device_guid);
    if(res != ADB_ERR_OK)
        return res;

    guid_len = strlen(device_guid) + 1;
    if(out_guid)
    {
        if(out_guid_len < guid_len)
            return ADB_ERR_TOO_SMALL;
        else
            memcpy(out_guid, device_guid, guid_len);
    }

    ADB__INFO("pairing wireless device guid=\"%s\" successfully",
            device_guid);
    return ADB_ERR_OK;
}

static adb_error_t adb__qr_random_string(
        adb_ctx_t *ctx,
        uint8_t *out,
        const size_t size)
{
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_";

    int err = 0;

    if(!ctx || !out || size == 0)
        return ADB_ERR_PARAM;

    err = mbedtls_ctr_drbg_random(
            &ctx->drbg,
            out,
            size);
    if(err != 0)
        return ADB_ERR_CRYPTO;

    for(size_t i = 0; i < size; i++)
        out[i] = charset[out[i] & 0x3FU];

    return ADB_ERR_OK;
}

adb_error_t adb_pair_qr_build_payload(
        adb_ctx_t *ctx,
        char *out_service,
        const size_t service_size,
        char *out_secret,
        const size_t secret_size)
{
    adb_error_t res = ADB_ERR_OK;
    uint8_t random[ADB__QR_RANDOM_LENGTH] = {0};
    int written = 0;

    if(!ctx || !out_service || service_size == 0 ||
            !out_secret || secret_size == 0)
        return ADB_ERR_PARAM;

    if(service_size <= strlen("studio-") + ADB__QR_RANDOM_LENGTH)
        return ADB_ERR_TOO_SMALL;

    if(secret_size <= ADB__QR_RANDOM_LENGTH)
        return ADB_ERR_TOO_SMALL;

    res = adb__qr_random_string(ctx, random, sizeof(random));
    if(res != ADB_ERR_OK)
        return res;

    written = snprintf(
            out_service,
            service_size,
            "studio-%." ADB__STRINGIFY(ADB__QR_RANDOM_LENGTH) "s",
            random);
    if(written < 0 || (size_t)written >= service_size)
        return ADB_ERR_TOO_SMALL;

    res = adb__qr_random_string(ctx, random, sizeof(random));
    if(res != ADB_ERR_OK)
        return res;

    memcpy(out_secret, random, sizeof(random));
    out_secret[sizeof(random)] = '\0';

    return ADB_ERR_OK;
}

adb_error_t adb_pair_qr_encode_payload(
        const char *service_name,
        const char *secret,
        char *out,
        const size_t size)
{
    int written = 0;
    if(!service_name || !secret || !out || size == 0)
        return ADB_ERR_PARAM;

    written = snprintf(out, size,
            "WIFI:T:ADB;S:%s;P:%s;;",
            service_name, secret);
    if(written < 0 || (size_t)written >= size)
        return ADB_ERR_TOO_SMALL;
    
    return ADB_ERR_OK;
}

adb_error_t adb_pair_qr(
        adb_conn_t *conn,
        const char *secret,
        adb_key_t *key,
        char *out_guid,
        const size_t out_guid_len)
{
    adb_error_t res = ADB_ERR_OK;

    size_t secret_len = 0;
    size_t password_len = 0;
    uint8_t *password = NULL;
    void *p = NULL;

    uint8_t key_material[SPAKE2_MAX_KEY_LENGTH] = {0};
    size_t key_material_len = 0;
    char device_guid[ADB__MEMSZ(adb__peer_info_t, data)] = {0};
    size_t guid_len = 0;

    /* we do not enforce secret len */
    if(!conn || !secret || 
            (out_guid == 0 ^ out_guid_len == 0))
        return ADB_ERR_PARAM;

    ADB__INFO("pairing wireless device with secret");
    res = adb__conn_upgrade_tls(conn, key);
    if(res != ADB_ERR_OK)
        return res;
    
    secret_len = strlen(secret);
    password_len = secret_len + ADB__TLS_EXPORTED_KEY_LENGTH;
    password = adb__malloc(password_len);
    if(!password)
        return ADB_ERR_NO_MEM;

    p = adb__mempcpy(password, secret, secret_len);
    res = adb__tls_export_keying_material(
            adb__conn_get_tls(conn), p);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__exchange_message(
            conn, 
            password,
            password_len,
            key_material, 
            &key_material_len);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__exchange_info(
            conn, 
            key_material,
            key_material_len,
            key,
            device_guid);
    if(res != ADB_ERR_OK)
        return res;

    guid_len = strlen(device_guid) + 1;
    if(out_guid)
    {
        if(out_guid_len < guid_len)
            return ADB_ERR_TOO_SMALL;
        else
            memcpy(out_guid, device_guid, guid_len);
    }

    ADB__INFO("pairing wireless device guid=\"%s\" successfully",
            device_guid);
    return ADB_ERR_OK;

}
