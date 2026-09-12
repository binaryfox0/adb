#include <adb/adb_pair.h>

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

#define ADB__ENUM_KEY_VALUE(val) [(val)] = #val 

#define ADB__PAIR_CODE_DIGITS 6
#define ADB__PAIR_HEADER_VER     1
#define ADB__PAIR_HEADER_MIN_VER 1
#define ADB__PAIR_HEADER_MAX_VER 1
#define ADB__MAX_PEER_INFO_SIZE     8192
#define ADB__MAX_PAIR_PAYLOAD_SIZE       (ADB__MAX_PEER_INFO_SIZE * 2)

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
    void *payload;
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
        const char *code,
        const size_t code_len)
{
    if(!code && code_len != ADB__PAIR_CODE_DIGITS)
        return false;
    for(size_t i = 0; i < code_len; i++)
    {
        if(!isdigit(code[i]))
            return false;
    }
    return true;
}

static adb_error_t adb__pair_read_packet(
        adb_conn_t *conn,
        adb__pair_packet_t *pkt,
        void **out_buf)
{
    static const size_t version_offset =
        offsetof(adb__pair_packet_t, version);
    static const size_t type_offset =
        offsetof(adb__pair_packet_t, type);
    static const size_t plsz_offset = 
        offsetof(adb__pair_packet_t, size);
    uint8_t buffer[sizeof(*pkt) - sizeof(void*)] = {0};
    adb_error_t res = ADB_ERR_OK;
    uint8_t version = 0;
    uint8_t type = 0;

    if(!conn || !pkt)
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

    pkt->type = type;
    pkt->version = version;
    pkt->size = 
        (uint32_t)(buffer[plsz_offset] << 24) |
        (uint32_t)(buffer[plsz_offset + 1] << 16) |
        (uint32_t)(buffer[plsz_offset + 2] << 8) |
        (uint32_t)buffer[plsz_offset + 3];

    if(!ADB__IN_RANGE(pkt->size, 1, 
                ADB__MAX_PAIR_PAYLOAD_SIZE))
    {
        ADB__ERROR("pairing paylod size not within a safe range");
        ADB__INFO("range: [%d, %d], got: %u bytes",
                1, ADB__MAX_PAIR_PAYLOAD_SIZE,
                pkt->size);
        return ADB_ERR_PROTOCOL;
    }

    *out_buf = adb__malloc(pkt->size);
    if(!*out_buf)
        return ADB_ERR_NO_MEM;

    res = adb__conn_read(conn, *out_buf, pkt->size);
    if(res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to read pairing payload");
        return res;
    }

    ADB__INFO("read pairing packet sucessfully");
    ADB__DEBUG("version: 0x%02X, type: 0x%02X (%s), size: %u bytes",
            pkt->version, pkt->type, adb__pair_packet_type_strings[pkt->type],
            pkt->size);
    return ADB_ERR_OK;
}

static adb_error_t adb__pair_write_packet(
        adb_conn_t *conn,
        adb__pair_packet_t *pkt)
{
    static const size_t version_offset =
        offsetof(adb__pair_packet_t, version);
    static const size_t type_offset =
        offsetof(adb__pair_packet_t, type);
    static const size_t plsz_offset = 
        offsetof(adb__pair_packet_t, size);
    uint8_t buffer[sizeof(*pkt) - sizeof(void*)] = {0};
    adb_error_t res = ADB_ERR_OK;

    if(!conn || !pkt)
        return ADB_ERR_PARAM;

    ADB__INFO("writing pairing packet");
    ADB__DEBUG("version: 0x%02X, type: 0x%02X (%s), size: %u bytes",
            pkt->version, pkt->type, adb__pair_packet_type_strings[pkt->type],
            pkt->size);

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

    res = adb__conn_write(conn, pkt->payload, pkt->size);
    if(res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to write pairing payload");
        return res;
    }

    ADB__INFO("wrote pairing packet sucessfully");
    return ADB_ERR_OK;
}

static bool adb__pair_check_packet(
        adb__pair_packet_t *pkt,
        const adb__pair_packet_type_t type,
        const uint32_t size)
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
    
    if(pkt->size != size)
    {
        ADB__ERROR("unexpected pairing payload size");
        ADB__INFO("expected: %u bytes, got %u bytes",
                size, pkt->size);
        return false;
    }

    return true;
}

adb_error_t adb_pair(
        adb_ctx_t *ctx,
        const char *host,
        const uint16_t port,
        const char *code,
        const size_t code_len,
        adb_key_t *key)
{
    static const uint8_t client_name[] = "adb pair client";
    static const uint8_t server_name[] = "adb pair server";

    adb_error_t ret = ADB_ERR_OK;
    adb_conn_t *conn = NULL;
    adb__tls_t *tls = NULL;
    uint8_t private_key[4096] = {0};
    uint8_t x509_cert[4096] = {0};

    spake2_ctx_t *spake2 = NULL;
    int err = 0;
    uint8_t random_data[SPAKE2_RANDOM_DATA_LENGTH] = {0};
    uint8_t password[ADB__PAIR_CODE_DIGITS + ADB__TLS_EXPORTED_KEY_LENGTH] = {0};
    uint8_t my_msg[SPAKE2_MAX_MESSAGE_LENGTH] = {0};
    size_t my_msg_size = 0;
    
    adb__pair_packet_t pkt = {0};
    uint8_t *their_msg = NULL;
    uint8_t key_material[SPAKE2_MAX_KEY_LENGTH] = {0};
    size_t key_material_len = 0;
    adb__aes_t aes = {0};
    adb__peer_info_t my_info = {0};
    uint8_t enc_my_info[ADB__AES_ENCYPTED_SIZE(sizeof(my_info))] = {0};

    uint8_t *enc_their_info = NULL;
    adb__peer_info_t their_info = {0};
    if(!ctx || !adb__verify_pairing_code(code, code_len))
        return ADB_ERR_PARAM;

    ADB__INFO("pairing wireless device with code \"%6s\"", code);
    ret = adb_conn_create_wireless(&conn, ctx, host, port);
    if(ret != ADB_ERR_OK)
        return ret;
    
    tls = adb__conn_get_tls(conn); 
    ret = adb__tls_handshake(tls, ctx, key);
    if(ret != ADB_ERR_OK)
        return ret;

    if(
            !adb__key_write_x509_pem(key, ctx, 
                x509_cert, sizeof(x509_cert)) ||
            !adb__key_write_pkcs8_pem(key, 
                private_key, sizeof(private_key)))
    {
        ADB__ERROR("failed to create X509/PKCS#8 PEM");
        return ADB_ERR_CRYPTO;
    }
    
    memcpy(password, code, ADB__PAIR_CODE_DIGITS);
    ret = adb__tls_export_keying_material(tls,
            password + ADB__PAIR_CODE_DIGITS);
    if(ret != ADB_ERR_OK)
        return ret;

    
    adb__log_print_payload(
            password, 
            sizeof(password), 
            "spake2 password");

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
    
    err = mbedtls_ctr_drbg_random(
            &ctx->drbg,
            random_data,
            sizeof(random_data));
    if(err != 0)
    {
        adb__log_err_mbedtls(err, 
                "failed to generate random data to generate message");
        goto cleanup;
    }

    if(!spake2_generate_msg(
            spake2, 
            my_msg, 
            &my_msg_size, 
            sizeof(my_msg),
            password, 
            sizeof(password),
            random_data))
    {
        ADB__ERROR("failed to generate SPAKE2 message");
        goto cleanup;
    }

    pkt.version = ADB__PAIR_HEADER_VER;
    pkt.type = ADB__PAIR_PACKET_SPAKE2;
    pkt.size = (uint32_t)my_msg_size;
    pkt.payload = my_msg;

    ret = adb__pair_write_packet(conn, &pkt);
    if(ret != ADB_ERR_OK)
        goto cleanup;

    ret = adb__pair_read_packet(conn, &pkt, (void**)&their_msg);
    if(ret != ADB_ERR_OK)
        goto cleanup;
    if(!adb__pair_check_packet(&pkt, ADB__PAIR_PACKET_SPAKE2, 
                SPAKE2_MAX_MESSAGE_LENGTH))
    {
        ret = ADB_ERR_PROTOCOL;
        goto cleanup;
    }

    if(!spake2_process_msg(
                spake2, 
                key_material, 
                &key_material_len, 
                sizeof(key_material),
                their_msg, 
                pkt.size))
    {
        ADB__ERROR("failed to process their public key");
        ret = ADB_ERR_CRYPTO; // XXX: should be another err since
                              // it came from the device
        goto cleanup;
    }

    ret = adb__aes_init(&aes, key_material, key_material_len);
    my_info.type = ADB__PEER_INFO_PUBLIC_KEY; 
    ret = adb_key_generate_pubkey(
            key, 
            my_info.data, 
            sizeof(my_info.data), 
            NULL);
    if(ret != ADB_ERR_OK)
        goto cleanup;
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
    pkt.payload = enc_my_info;

    ret = adb__pair_write_packet(conn, &pkt);
    if(ret != ADB_ERR_OK)
        goto cleanup; 
   
    ret = adb__pair_read_packet(conn, &pkt, 
            (void**)&enc_their_info);
    if(ret != ADB_ERR_OK)
        goto cleanup; 
    if(!adb__pair_check_packet(&pkt, ADB__PAIR_PACKET_PEER_INFO, 
                ADB__AES_ENCYPTED_SIZE(sizeof(their_info))))
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

    adb__log_print_payload(
            &their_info, sizeof(their_info), "their info");
    
    ADB__INFO("pairing wireless device successfully");

cleanup:
    adb__aes_destroy(&aes);
    spake2_ctx_free(spake2);
    adb__free(their_msg);
    return ret;
}
