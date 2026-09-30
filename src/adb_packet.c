#include "adb_packet.h"

#include "adb_conn_priv.h"
#include "adb_log_priv.h"
#include "adb_utils.h"
#include "adb_alloc_priv.h"

typedef struct 
{
    uint32_t command;      /* command identifier constant      */
    uint32_t arg0;         /* first argument                   */
    uint32_t arg1;         /* second argument                  */
    uint32_t payload_size; /* payload size (0 is allowed) */
    uint32_t data_check;   /* checksum of payload         */
    uint32_t magic;        /* command ^ 0xffffffff             */
} adb__packet_serialized_t;

static void adb__log_packet(
        const adb__packet_t *pkt)
{
    ADB__DEBUG("pkt: command: 0x%08X (%.4s), "
            "arg0: 0x%08X, arg1=0x%08X, size=0x%08X bytes",
            pkt->command, (const char*)&pkt->command,
            pkt->arg0, pkt->arg1, pkt->payload_size);
}

adb_error_t adb__packet_write(
        adb_conn_t *conn,
        const adb__packet_t *pkt,
        const void *payload)
{
    adb_error_t res = ADB_ERR_OK;
    const uint8_t *payload_data = NULL;
    uint32_t sum = 0;
    adb__packet_serialized_t spkt = {0};
    if(!conn || !pkt)
        return ADB_ERR_PARAM;

    ADB__INFO("writing packet");
    adb__log_packet(pkt);

    payload_data = payload;
    for(uint32_t i = 0; i < pkt->payload_size; i++)
        sum += payload_data[i];

    spkt.command = pkt->command;
    spkt.arg0 = pkt->arg0;
    spkt.arg1 = pkt->arg1;
    spkt.payload_size = pkt->payload_size;
    spkt.data_check = sum;
    spkt.magic = pkt->command ^ 0xffffffff;

    res = adb__conn_write(conn, &spkt, sizeof(spkt));
    if(res != ADB_ERR_OK)
        return res;

    if(pkt->payload_size == 0)
        return ADB_ERR_OK;

    res = adb__conn_write(conn, payload,  pkt->payload_size);
    if(res != ADB_ERR_OK)
        return res;

    ADB__INFO("wrote packet successfully");
    return ADB_ERR_OK;
}

static inline bool adb__packet_verify(
        const uint32_t max_size,
        const adb__packet_serialized_t *spkt)
{
    if(spkt->payload_size > ADB__PACKET_MAX_PAYLOAD_SIZE)
    {
        ADB__ERROR("payload size exceeds the maximum allowed size");
        ADB__INFO("max size: %d bytes, got: %u bytes",
                ADB__PACKET_MAX_PAYLOAD_SIZE,
                spkt->payload_size);
        return ADB_ERR_PROTOCOL;
    }

    if(spkt->payload_size > max_size)
    {
        ADB__ERROR("payload size exceeds the maximum exchanged size");
        ADB__INFO("max size: %u bytes, got: %u bytes",
                max_size, spkt->payload_size);
        return false;
    }

    if(spkt->magic != (spkt->command ^ 0xffffffff))
    {
        ADB__ERROR("respond packet command magic mismatch");
        ADB__INFO("expected: 0x%08X, got: 0x%08X",
                spkt->command ^ 0xffffffff, spkt->magic);
        return false;
    }
    return true;
}

ADB__INLINE adb__packet_t adb__pkt_from_spkt(
        const adb__packet_serialized_t *spkt)
{
    adb__packet_t pkt = {0};
    pkt.command = spkt->command;
    pkt.arg0 = spkt->arg0;
    pkt.arg1 = spkt->arg1;
    pkt.payload_size = spkt->payload_size;
    return pkt;
}

adb_error_t adb__packet_read(
        adb_conn_t *conn,
        adb__packet_t *out_pkt,
        void **out_payload)
{
    adb_error_t res = ADB_ERR_OK;
    adb__packet_serialized_t spkt = {0};
    void *tmp_payload = NULL;
    if(!conn || !out_pkt)
        return ADB_ERR_PARAM;

    ADB__INFO("reading packet");

    res = adb__conn_read(conn, &spkt, sizeof(spkt));
    if(res != ADB_ERR_OK)
        return res;

    if(!adb__packet_verify(
                adb__conn_get_max_payload_size(conn),
                &spkt))
        return ADB_ERR_PROTOCOL;

    if(spkt.payload_size == 0)
        goto success;

    tmp_payload = adb__malloc(spkt.payload_size);
    if(!tmp_payload)
        return ADB_ERR_NO_MEM;
    
    res = adb__conn_read(conn, tmp_payload, spkt.payload_size);
    if(res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to read payload body");
        adb__free(tmp_payload);
        return res;
    }

success:
    *out_pkt = adb__pkt_from_spkt(&spkt);
    *out_payload = tmp_payload;
    
    ADB__INFO("read packet successfully");
    adb__log_packet(out_pkt);

    return ADB_ERR_OK;
}

adb_error_t adb__packet_read_into(
        adb_conn_t *conn,
        adb__packet_t *out_pkt,
        void *out_payload,
        const size_t max_size)
{
    adb_error_t res = ADB_ERR_OK;
    adb__packet_serialized_t spkt = {0};
    if(!conn || !out_pkt)
        return ADB_ERR_PARAM;

    ADB__INFO("reading packet");

    res = adb__conn_read(conn, &spkt, sizeof(spkt));
    if(res != ADB_ERR_OK)
        return res;

    if(!adb__packet_verify(
                adb__conn_get_max_payload_size(conn),
                &spkt))
        return ADB_ERR_PROTOCOL;

    if(spkt.payload_size == 0)
        goto success;
    if(spkt.payload_size > max_size)
    {
        ADB__ERROR("payload size exceeds maximum requested size");
        ADB__INFO("max size: %zu bytes, got: %u bytes",
                max_size, spkt.payload_size);
        return ADB_ERR_PROTOCOL;
    }

    res = adb__conn_read(conn, out_payload, spkt.payload_size);
    if(res != ADB_ERR_OK)
    {
        adb__log_err_adb(res, "failed to read payload body");
        return res;
    }

success:
    *out_pkt = adb__pkt_from_spkt(&spkt);
    
    ADB__INFO("read packet successfully");
    adb__log_packet(out_pkt);

    return ADB_ERR_OK;
}

/*
 * XXX: idk what to do for read packet with timeout
 * the current implementation use timeout_ms as timeout
 * for both the packet header and payload (if header was read, 
 * but payload timed out), not timeout for entire packet
 */
adb_error_t adb__packet_read_timeout(
        adb_conn_t *conn,
        adb__packet_reader_t *reader,
        const uint32_t timeout_ms)
{
    adb_error_t res = ADB_ERR_OK;
    uint64_t deadline = 0;
    uint64_t now = 0;
    if(!conn || !reader)
        return ADB_ERR_PARAM;

    deadline = adb__util_monotonic_ms() + timeout_ms;
    if(!reader->header_ready)
    {
        adb__packet_serialized_t spkt = {0};
        res = adb__conn_read_timeout(conn, 
                &spkt, sizeof(spkt), timeout_ms);
        if(res != ADB_ERR_OK)
            return res;

        if(!adb__packet_verify(
                    adb__conn_get_max_payload_size(conn),
                    &spkt))
            return ADB_ERR_PROTOCOL;

        reader->pkt = adb__pkt_from_spkt(&spkt);
        reader->header_ready = true;
        if(reader->pkt.payload_size == 0)
            return ADB_ERR_OK;
    }

    now = adb__util_monotonic_ms();
    if(now > deadline)
        return ADB_ERR_TIMEOUT;

    if(!reader->payload)
    {
        /* assign to the reader to reuse if read failed */
        reader->payload = adb__malloc(reader->pkt.payload_size);
        if(!reader->payload)
            return ADB_ERR_NO_MEM;
    }

    res = adb__conn_read_timeout(conn, 
            reader->payload, 
            reader->pkt.payload_size, 
            (uint32_t)(deadline - now));
    if(res != ADB_ERR_OK)
        return res;

    return ADB_ERR_OK;
}

void adb__packet_reader_reset(
        adb__packet_reader_t *reader)
{
    if(!reader)
        return;
    adb__free(reader->payload);
    *reader = (adb__packet_reader_t){0};
}

bool adb__packet_check_cmd(
        const adb__packet_t *pkt,
        const uint32_t expected)
{
    bool ret = false;
    if(!pkt)
        return false;
    
    ret = pkt->command == expected;
    if(!ret)
    {
        ADB__ERROR("unexpected packet type");
        ADB__INFO("expected: 0x%08X (%.4s), got: 0x%08X (%.4s)",
                expected, (const char*)&expected,
                pkt->command, (const char*)&pkt->command);
    }
    return ret;
}

bool adb__packet_check_size(
        const adb__packet_t *pkt,
        const uint32_t size)
{
    bool ret = false;
    if(!pkt)
        return false;
    
    ret = pkt->payload_size == size;
    if(!ret)
    {
        ADB__ERROR("unexpected packet payload size");
        ADB__INFO("expected: %u bytes, got: %u bytes",
                size, pkt->payload_size);
    }
    return ret;
}
