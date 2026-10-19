#include <adb/adb_pair.h>

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>

#include <psa/crypto.h>
#include <spake2.h>

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

#define ADB__PAIR_CODE_DIGITS       6U
#define ADB__PAIR_HEADER_VER        1U
#define ADB__PAIR_HEADER_MIN_VER    1U
#define ADB__PAIR_HEADER_MAX_VER    1U
#define ADB__MAX_PEER_INFO_SIZE     8192U
#define ADB__PAIR_MAX_PAYLOAD_SIZE  (ADB__MAX_PEER_INFO_SIZE * 2U)
#define ADB__SPAKE2_PASSWORD_LENGTH \
    (ADB__PAIR_CODE_DIGITS + ADB__TLS_EXPORTED_KEY_LENGTH)
#define ADB__QR_RANDOM_LENGTH       10

typedef enum
{
    ADB__PAIR_PACKET_SPAKE2,
    ADB__PAIR_PACKET_PEER_INFO,
    ADB__PAIR_PACKET_COUNT,
} adb__pair_packet_type_t;

typedef struct __attribute__((packed))
{
    uint8_t version;
    uint8_t type;
    uint32_t size;
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
    uint8_t data[ADB__MAX_PEER_INFO_SIZE - 1U];
} adb__peer_info_t;

static const char *adb__pair_packet_type_strings[ADB__PAIR_PACKET_COUNT] =
{
    ADB__ENUM_KEY_VALUE(ADB__PAIR_PACKET_SPAKE2),
    ADB__ENUM_KEY_VALUE(ADB__PAIR_PACKET_PEER_INFO)
};

static const char *adb__peer_info_type_strings[ADB__PEER_INFO_COUNT] =
{
    ADB__ENUM_KEY_VALUE(ADB__PEER_INFO_PUBLIC_KEY),
    ADB__ENUM_KEY_VALUE(ADB__PEER_INFO_DEVICE_GUID)
};

static adb_error_t adb__random_bytes(void *out, const size_t size)
{
    psa_status_t status = PSA_SUCCESS;
    if (!out || size == 0U)
        return ADB_ERR_PARAM;

    status = psa_generate_random(out, size);
    if (status != PSA_SUCCESS)
    {
        ADB__ERROR("random generation failed");
        ADB__INFO("reason: status %d", (int)status);
        return ADB_ERR_CRYPTO;
    }

    return ADB_ERR_OK;
}

static inline bool adb__verify_pairing_code(const char *code)
{
    if (!code || strlen(code) != ADB__PAIR_CODE_DIGITS)
        return false;

    for (size_t i = 0U; i < ADB__PAIR_CODE_DIGITS; i++)
    {
        if (!isdigit((unsigned char)code[i]))
            return false;
    }
    return true;
}

static void adb__log_pair_packet(const adb__pair_packet_t *pkt)
{
    if (!pkt)
        return;

    if (pkt->type < ADB__PAIR_PACKET_COUNT)
    {
        ADB__DEBUG("ppkt: version=0x%02X, type=0x%02X (%s), size=0x%08X bytes",
                pkt->version, pkt->type,
                adb__pair_packet_type_strings[pkt->type], pkt->size);
    }
    else
    {
        ADB__DEBUG("ppkt: version=0x%02X, type=0x%02X, size=0x%08X bytes",
                pkt->version, pkt->type, pkt->size);
    }
}

static adb_error_t adb__pair_read_packet(
        adb_conn_t *conn,
        adb__pair_packet_t *out_pkt,
        void *out_payload,
        const size_t size)
{
    static const size_t version_offset = offsetof(adb__pair_packet_t, version);
    static const size_t type_offset = offsetof(adb__pair_packet_t, type);
    static const size_t plsz_offset = offsetof(adb__pair_packet_t, size);
    uint8_t buffer[sizeof(*out_pkt)] = {0};
    adb_error_t res = ADB_ERR_OK;
    uint8_t version = 0U;
    uint8_t type = 0U;
    uint32_t dec_size = 0U;

    if (!conn || !out_pkt)
        return ADB_ERR_PARAM;

    ADB__INFO("reading pairing packet");

    res = adb__conn_read(conn, buffer, sizeof(buffer));
    if (res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to read pairing header");
        return res;
    }

    version = buffer[version_offset];
    if (!ADB__IN_RANGE(version,
                ADB__PAIR_HEADER_MIN_VER, ADB__PAIR_HEADER_MAX_VER))
    {
        ADB__ERROR("unsupported pairing header version");
        ADB__INFO("supported version range %d,%d, got: %d",
                (int)ADB__PAIR_HEADER_MIN_VER,
                (int)ADB__PAIR_HEADER_MAX_VER, (int)version);
        return ADB_ERR_UNSUPPORTED;
    }

    type = buffer[type_offset];
    if (type >= ADB__PAIR_PACKET_COUNT)
    {
        ADB__ERROR("unsupported pairing header type");
        ADB__INFO("received pairing packet type: 0x%02X", type);
        return ADB_ERR_UNSUPPORTED;
    }

    dec_size = ((uint32_t)buffer[plsz_offset] << 24U) |
            ((uint32_t)buffer[plsz_offset + 1U] << 16U) |
            ((uint32_t)buffer[plsz_offset + 2U] << 8U) |
            (uint32_t)buffer[plsz_offset + 3U];

    if (!ADB__IN_RANGE(dec_size, 1U, ADB__PAIR_MAX_PAYLOAD_SIZE))
    {
        ADB__ERROR("pairing payload size not within a safe range");
        ADB__INFO("safe range: [%u, %u], got: %u bytes",
                1U, ADB__PAIR_MAX_PAYLOAD_SIZE, dec_size);
        return ADB_ERR_PROTOCOL;
    }

    if ((size_t)dec_size > size || !out_payload)
    {
        ADB__ERROR("pairing payload size exceeds the output buffer");
        ADB__INFO("max size: %zu bytes, got: %u bytes", size, dec_size);
        return ADB_ERR_PROTOCOL;
    }

    res = adb__conn_read(conn, out_payload, (size_t)dec_size);
    if (res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to read pairing payload");
        return res;
    }

    out_pkt->version = version;
    out_pkt->type = type;
    out_pkt->size = dec_size;

    ADB__INFO("read pairing packet successfully");
    adb__log_pair_packet(out_pkt);
    return ADB_ERR_OK;
}

static adb_error_t adb__pair_write_packet(
        adb_conn_t *conn,
        adb__pair_packet_t *pkt,
        const void *payload)
{
    static const size_t version_offset = offsetof(adb__pair_packet_t, version);
    static const size_t type_offset = offsetof(adb__pair_packet_t, type);
    static const size_t plsz_offset = offsetof(adb__pair_packet_t, size);
    uint8_t buffer[sizeof(*pkt)] = {0};
    adb_error_t res = ADB_ERR_OK;

    if (!conn || !pkt || (pkt->size != 0U && !payload) ||
            pkt->type >= ADB__PAIR_PACKET_COUNT)
        return ADB_ERR_PARAM;

    ADB__INFO("writing pairing packet");
    adb__log_pair_packet(pkt);

    buffer[version_offset] = pkt->version;
    buffer[type_offset] = pkt->type;
    buffer[plsz_offset] = (uint8_t)((pkt->size & 0xFF000000U) >> 24U);
    buffer[plsz_offset + 1U] = (uint8_t)((pkt->size & 0x00FF0000U) >> 16U);
    buffer[plsz_offset + 2U] = (uint8_t)((pkt->size & 0x0000FF00U) >> 8U);
    buffer[plsz_offset + 3U] = (uint8_t)(pkt->size & 0x000000FFU);

    res = adb__conn_write(conn, buffer, sizeof(buffer));
    if (res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to write pairing header");
        return res;
    }

    res = adb__conn_write(conn, payload, pkt->size);
    if (res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to write pairing payload");
        return res;
    }

    ADB__INFO("wrote pairing packet successfully");
    return ADB_ERR_OK;
}

static inline bool adb__pair_check_packet(
        adb__pair_packet_t *pkt,
        const adb__pair_packet_type_t type)
{
    if (!pkt || type >= ADB__PAIR_PACKET_COUNT)
        return false;

    if (pkt->type != (uint8_t)type)
    {
        ADB__ERROR("unexpected pairing header type");
        ADB__INFO("expected: 0x%02X (%s), got: 0x%02X (%s)",
                type, adb__pair_packet_type_strings[type], pkt->type,
                pkt->type < ADB__PAIR_PACKET_COUNT ?
                    adb__pair_packet_type_strings[pkt->type] : "unknown");
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
    uint8_t random_data[SPAKE2_RANDOM_DATA_LENGTH] = {0};
    spake2_ctx_t *spake2 = NULL;
    uint8_t my_msg[SPAKE2_MAX_MESSAGE_LENGTH] = {0};
    size_t my_msg_size = 0U;
    adb__pair_packet_t pkt = {0};
    uint8_t their_msg[SPAKE2_MAX_MESSAGE_LENGTH] = {0};

    if (!conn || !password || password_len == 0U || !out_key_material ||
            !out_key_material_len)
        return ADB_ERR_PARAM;

    ADB__INFO("exchanging SPAKE2 message with the device");

    ret = adb__random_bytes(random_data, sizeof(random_data));
    if (ret != ADB_ERR_OK)
    {
        ADB__ERROR("failed to generate random data for SPAKE2 message");
        goto cleanup;
    }

    spake2 = spake2_ctx_create(
            &(spake2_allocator_t) {
                .malloc = adb__alloc_get()->malloc,
                .free = adb__alloc_get()->free,
                .userdata = adb__alloc_get()->userdata
            }, SPAKE2_ROLE_ALICE,
            client_name, sizeof(client_name),
            server_name, sizeof(server_name));
    if (!spake2)
    {
        ADB__ERROR("failed to create SPAKE2 context (out of memory)");
        ret = ADB_ERR_NO_MEM;
        goto cleanup;
    }

    if (!spake2_generate_msg(spake2, my_msg, &my_msg_size, sizeof(my_msg),
                password, password_len, random_data))
    {
        ADB__ERROR("failed to generate SPAKE2 message");
        ret = ADB_ERR_CRYPTO;
        goto cleanup;
    }

    pkt.version = ADB__PAIR_HEADER_VER;
    pkt.type = ADB__PAIR_PACKET_SPAKE2;
    pkt.size = (uint32_t)my_msg_size;

    ret = adb__pair_write_packet(conn, &pkt, my_msg);
    if (ret != ADB_ERR_OK)
        goto cleanup;

    ret = adb__pair_read_packet(conn, &pkt, their_msg, sizeof(their_msg));
    if (ret != ADB_ERR_OK)
        goto cleanup;
    if (!adb__pair_check_packet(&pkt, ADB__PAIR_PACKET_SPAKE2))
    {
        ret = ADB_ERR_PROTOCOL;
        goto cleanup;
    }

    if (!spake2_process_msg(spake2, out_key_material,
                out_key_material_len, SPAKE2_MAX_KEY_LENGTH,
                their_msg, pkt.size))
    {
        ADB__ERROR("failed to process peer SPAKE2 message");
        ret = ADB_ERR_CRYPTO;
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
    uint8_t enc_my_info[ADB__AES_ENCRYPTED_SIZE(sizeof(my_info))] = {0};
    adb__pair_packet_t pkt = {0};
    uint8_t enc_their_info[ADB__AES_ENCRYPTED_SIZE(sizeof(my_info))] = {0};
    adb__peer_info_t their_info = {0};
    bool has_null = false;

    if (!conn || !key_material || key_material_len == 0U || !key || !out_guid)
        return ADB_ERR_PARAM;

    ADB__INFO("exchanging info with device");

    ret = adb__aes_init(&aes, key_material, key_material_len);
    if (ret != ADB_ERR_OK)
        goto cleanup;

    my_info.type = ADB__PEER_INFO_PUBLIC_KEY;
    ret = adb_key_generate_pubkey(key, (char*)my_info.data, sizeof(my_info.data), NULL);
    if (ret != ADB_ERR_OK)
        goto cleanup;

    adb__log_multiline_text((const char *)my_info.data, "public key");

    ret = adb__aes_encrypt(&aes, &my_info, sizeof(my_info),
            enc_my_info, sizeof(enc_my_info));
    if (ret != ADB_ERR_OK)
        goto cleanup;

    pkt.version = ADB__PAIR_HEADER_VER;
    pkt.type = ADB__PAIR_PACKET_PEER_INFO;
    pkt.size = (uint32_t)sizeof(enc_my_info);

    ret = adb__pair_write_packet(conn, &pkt, enc_my_info);
    if (ret != ADB_ERR_OK)
        goto cleanup;

    ret = adb__pair_read_packet(conn, &pkt, enc_their_info,
            sizeof(enc_their_info));
    if (ret != ADB_ERR_OK)
        goto cleanup;
    if (!adb__pair_check_packet(&pkt, ADB__PAIR_PACKET_PEER_INFO))
    {
        ret = ADB_ERR_PROTOCOL;
        goto cleanup;
    }
    if (pkt.size != sizeof(enc_their_info))
    {
        ADB__ERROR("unexpected peer info payload size: %u", pkt.size);
        ret = ADB_ERR_PROTOCOL;
        goto cleanup;
    }

    ret = adb__aes_decrypt(&aes, enc_their_info, pkt.size,
            &their_info, sizeof(their_info));
    if (ret != ADB_ERR_OK)
        goto cleanup;

    adb__log_payload(&their_info, sizeof(their_info), "their info");
    if (their_info.type >= ADB__PEER_INFO_COUNT)
    {
        ADB__ERROR("unsupported peer info type");
        ADB__INFO("received peer info type: 0x%02X", their_info.type);
        ret = ADB_ERR_UNSUPPORTED;
        goto cleanup;
    }
    if (their_info.type != ADB__PEER_INFO_DEVICE_GUID)
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

    for (size_t i = 0U; i < sizeof(their_info.data); i++)
    {
        if (their_info.data[i] == '\0')
        {
            has_null = true;
            break;
        }
    }

    if (!has_null)
    {
        ADB__ERROR("peer info is malformed: missing NUL terminator");
        ret = ADB_ERR_PROTOCOL;
        goto cleanup;
    }

    memcpy(out_guid, their_info.data, sizeof(their_info.data));
    ADB__INFO("exchanged info with device guid=\"%s\" successfully", out_guid);

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
    psa_status_t status = PSA_SUCCESS;
    uint8_t password[ADB__SPAKE2_PASSWORD_LENGTH] = {0};
    void *p = NULL;
    uint8_t key_material[SPAKE2_MAX_KEY_LENGTH] = {0};
    size_t key_material_len = 0U;
    char device_guid[ADB__MEMSZ(adb__peer_info_t, data)] = {0};
    size_t guid_len = 0U;

    if (!conn || !key || !adb__verify_pairing_code(code) ||
            ((out_guid == NULL) != (out_guid_len == 0U)))
        return ADB_ERR_PARAM;

    status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        ADB__ERROR("PSA crypto initialization failed (status %d)", (int)status);
        return ADB_ERR_CRYPTO;
    }

    ADB__INFO("pairing wireless device with code");
    res = adb__conn_upgrade_tls(conn, key);
    if (res != ADB_ERR_OK)
        return res;

    p = adb__mempcpy(password, code, ADB__PAIR_CODE_DIGITS);
    res = adb__tls_export_keying_material(adb__conn_get_tls(conn), p);
    if (res != ADB_ERR_OK)
        return res;

    res = adb__exchange_message(conn, password, sizeof(password),
            key_material, &key_material_len);
    if (res != ADB_ERR_OK)
        return res;

    res = adb__exchange_info(conn, key_material, key_material_len, key,
            device_guid);
    if (res != ADB_ERR_OK)
        return res;

    guid_len = strlen(device_guid) + 1U;
    if (out_guid)
    {
        if (out_guid_len < guid_len)
            return ADB_ERR_TOO_SMALL;
        memcpy(out_guid, device_guid, guid_len);
    }

    ADB__INFO("pairing wireless device guid=\"%s\" successfully", device_guid);
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
    adb_error_t ret = ADB_ERR_OK;

    if (!ctx || !out || size == 0U)
        return ADB_ERR_PARAM;

    ret = adb__random_bytes(out, size);
    if (ret != ADB_ERR_OK)
        return ret;

    for (size_t i = 0U; i < size; i++)
        out[i] = (uint8_t)charset[out[i] & 0x3FU];

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
    char random[ADB__QR_RANDOM_LENGTH + 1U] = {0};
    int written = 0;

    if (!ctx || !out_service || service_size == 0U ||
            !out_secret || secret_size == 0U)
        return ADB_ERR_PARAM;

    if (service_size <= strlen("studio-") + ADB__QR_RANDOM_LENGTH)
        return ADB_ERR_TOO_SMALL;
    if (secret_size <= ADB__QR_RANDOM_LENGTH)
        return ADB_ERR_TOO_SMALL;

    res = adb__qr_random_string(ctx, (uint8_t *)random, ADB__QR_RANDOM_LENGTH);
    if (res != ADB_ERR_OK)
        return res;
    random[ADB__QR_RANDOM_LENGTH] = '\0';

    written = snprintf(out_service, service_size, "studio-%s", random);
    if (written < 0 || (size_t)written >= service_size)
        return ADB_ERR_TOO_SMALL;

    res = adb__qr_random_string(ctx, (uint8_t *)random, ADB__QR_RANDOM_LENGTH);
    if (res != ADB_ERR_OK)
        return res;
    random[ADB__QR_RANDOM_LENGTH] = '\0';

    memcpy(out_secret, random, ADB__QR_RANDOM_LENGTH + 1U);
    return ADB_ERR_OK;
}

adb_error_t adb_pair_qr_encode_payload(
        const char *service_name,
        const char *secret,
        char *out,
        const size_t size)
{
    int written = 0;

    if (!service_name || !secret || !out || size == 0U)
        return ADB_ERR_PARAM;

    written = snprintf(out, size, "WIFI:T:ADB;S:%s;P:%s;;",
            service_name, secret);
    if (written < 0 || (size_t)written >= size)
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
    psa_status_t status = PSA_SUCCESS;
    size_t secret_len = 0U;
    size_t password_len = 0U;
    uint8_t *password = NULL;
    void *p = NULL;
    uint8_t key_material[SPAKE2_MAX_KEY_LENGTH] = {0};
    size_t key_material_len = 0U;
    char device_guid[ADB__MEMSZ(adb__peer_info_t, data)] = {0};
    size_t guid_len = 0U;

    if (!conn || !secret || !key ||
            ((out_guid == NULL) != (out_guid_len == 0U)))
        return ADB_ERR_PARAM;

    status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        ADB__ERROR("PSA crypto initialization failed (status %d)", (int)status);
        return ADB_ERR_CRYPTO;
    }

    ADB__INFO("pairing wireless device with secret");
    res = adb__conn_upgrade_tls(conn, key);
    if (res != ADB_ERR_OK)
        return res;

    secret_len = strlen(secret);
    if (secret_len > SIZE_MAX - ADB__TLS_EXPORTED_KEY_LENGTH)
        return ADB_ERR_TOO_LONG;
    password_len = secret_len + ADB__TLS_EXPORTED_KEY_LENGTH;
    if (password_len == 0U)
        return ADB_ERR_PARAM;

    password = adb__malloc(password_len);
    if (!password)
        return ADB_ERR_NO_MEM;

    p = adb__mempcpy(password, secret, secret_len);
    res = adb__tls_export_keying_material(adb__conn_get_tls(conn), p);
    if (res != ADB_ERR_OK)
        goto cleanup;

    res = adb__exchange_message(conn, password, password_len,
            key_material, &key_material_len);
    if (res != ADB_ERR_OK)
        goto cleanup;

    res = adb__exchange_info(conn, key_material, key_material_len, key,
            device_guid);
    if (res != ADB_ERR_OK)
        goto cleanup;

    guid_len = strlen(device_guid) + 1U;
    if (out_guid)
    {
        if (out_guid_len < guid_len)
        {
            res = ADB_ERR_TOO_SMALL;
            goto cleanup;
        }
        memcpy(out_guid, device_guid, guid_len);
    }

    ADB__INFO("pairing wireless device guid=\"%s\" successfully", device_guid);

cleanup:
    adb__free(password);
    return res;
}
