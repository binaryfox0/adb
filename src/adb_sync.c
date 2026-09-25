#include "adb_sync.h"

#include <stdint.h>
#include <string.h>

#include "adb_alloc_priv.h"
#include "adb_log_priv.h"
#include "adb_conn_priv.h"

#define ADB__INIT_DELAYED_ACK_BYTES (32 * 1024 * 1024)

adb_error_t adb__sync_open(
        adb__sync_t *sync,
        adb_conn_t *conn)
{
    adb_error_t res;

    if(!sync || !conn)
        return ADB_ERR_PARAM;

    memset(sync, 0, sizeof(*sync));

    sync->conn = conn;
    sync->delayed_ack = adb__has_feature(
            conn,
            ADB__FEATURE_DELAYED_ACK);
    sync->local_id = 1; /* should use a counter inside conn */
    // sync->send_ready = false;

    if(sync->delayed_ack)
        sync->our_asb = ADB__INIT_DELAYED_ACK_BYTES;

    sync->pkt.command = ADB__CMD_OPEN;
    sync->pkt.arg0 = sync->local_id;
    sync->pkt.arg1 = sync->delayed_ack ? ADB__INIT_DELAYED_ACK_BYTES : 0;
    sync->pkt.payload_size = 0;

    res = adb__packet_write(conn, &sync->pkt, NULL);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__packet_read_into(
            conn,
            &sync->pkt,
            &sync->their_asb,
            sync->delayed_ack ? sizeof(sync->their_asb) : 0);
    if(res != ADB_ERR_OK)
        return res;

    /* Peer rejected our delayed-ACK request. */
    if(sync->pkt.command == ADB__CMD_CLSE)
        return ADB_ERR_UNSUPPORTED;

    if(sync->pkt.command != ADB__CMD_OKAY)
        return ADB_ERR_PROTOCOL;

    sync->send_ready = true;
    sync->remote_id = sync->pkt.arg0;
    sync->opened = true;

    return ADB_ERR_OK;
}

adb_error_t adb__sync_write(
        adb__sync_t *sync,
        const void *payload,
        const size_t payload_size)
{
    adb_error_t res;

    if(!sync || !sync->opened ||
            (payload_size == 0) ^ (payload == NULL) ||
            payload_size > INT32_MAX)
        return ADB_ERR_PARAM;

    /*
     * Delayed ACK:
     *
     * We may send while our ASB is positive. Once it is exhausted,
     * wait for an A_OKAY to replenish it.
     */
    if(sync->delayed_ack)
    {
        while(sync->our_asb <= 0)
        {
            res = adb__sync_read(sync);
            if(res != ADB_ERR_OK)
                return res;

            if(!adb__packet_check_cmd(&sync->pkt, ADB__CMD_OKAY))
                return ADB_ERR_PROTOCOL;

            res = adb__sync_handle_okay(sync);
            if(res != ADB_ERR_OK)
                return res;

            continue;
        }
    } else {
        /* Legacy ADB allows only one WRTE in flight. */
        while(!sync->send_ready)
        {
            res = adb__sync_read(sync);
            if(res != ADB_ERR_OK)
                return res;

            if(sync->pkt.command == ADB__CMD_OKAY)
            {
                res = adb__sync_handle_okay(sync);
                if(res != ADB_ERR_OK)
                    return res;

                continue;
            }

            return ADB_ERR_PROTOCOL;
        }
    }

    sync->pkt.command = ADB__CMD_WRTE;
    sync->pkt.arg0 = sync->local_id;
    sync->pkt.arg1 = sync->remote_id;
    sync->pkt.payload_size = (uint32_t)payload_size;

    res = adb__packet_write(sync->conn, &sync->pkt, payload);
    if(res != ADB_ERR_OK)
        return res;

    if(sync->delayed_ack)
        sync->our_asb -= (int64_t)payload_size;
    else
        sync->send_ready = false;

    return ADB_ERR_OK;
}

adb_error_t adb__sync_read(
        adb__sync_t *sync)
{
    adb_error_t res = ADB_ERR_OK;
    if(!sync || !sync->opened)
        return ADB_ERR_PARAM;

    adb__free(sync->pkt_payload);
    sync->pkt_payload = NULL;

    res = adb__packet_read(
            sync->conn,
            &sync->pkt,
            (void**)&sync->pkt_payload);
    if(res != ADB_ERR_OK)
        return res;

    if(sync->pkt.arg0 != sync->remote_id ||
            sync->pkt.arg1 != sync->local_id)
    {
        ADB__ERROR("unexpected packet IDs");
        ADB__INFO(
                "expected local: %u, remote: %u",
                sync->local_id,
                sync->remote_id);
        ADB__INFO(
                "got local: %u, remote: %u",
                sync->pkt.arg1,
                sync->pkt.arg0);

        return ADB_ERR_PROTOCOL;
    }

    if(sync->pkt.command == ADB__CMD_CLSE)
        return ADB_ERR_DISCONNECTED;

    if(sync->pkt.command == ADB__CMD_WRTE &&
            sync->delayed_ack)
    {
        /*
         * The peer has consumed this much of its send ASB.
         * We replenish it later through A_OKAY.
         */
        sync->their_asb -= (int64_t)sync->pkt.payload_size;
    }

    return ADB_ERR_OK;
}

adb_error_t adb__sync_handle_okay(
        adb__sync_t *sync)
{
    if(!sync || !sync->opened)
        return ADB_ERR_PARAM;

    if(sync->pkt.command != ADB__CMD_OKAY)
        return ADB_ERR_PROTOCOL;

    if(sync->delayed_ack)
    {
        int32_t ack_bytes = 0;
        if(!adb__packet_check_size(
                    &sync->pkt,
                    sizeof(ack_bytes)))
            return ADB_ERR_PROTOCOL;

        memcpy(&ack_bytes, sync->pkt_payload, sizeof(ack_bytes));
        sync->our_asb += ack_bytes;
    } else {
        if(!adb__packet_check_size(&sync->pkt, 0))
            return ADB_ERR_PROTOCOL;

        sync->send_ready = true;
    }

    return ADB_ERR_OK;
}

adb_error_t adb__sync_ack(
        adb__sync_t *sync)
{
    adb_error_t res;
    if(!sync || !sync->opened)
        return ADB_ERR_PARAM;

    if(sync->pkt.command != ADB__CMD_WRTE)
        return ADB_ERR_PROTOCOL;

    sync->pkt.command = ADB__CMD_OKAY;
    sync->pkt.arg0 = sync->local_id;
    sync->pkt.arg1 = sync->remote_id;

    if(sync->delayed_ack)
    {
        sync->pending_bytes += (int32_t)sync->pkt.payload_size;

        /*
         * Don't send an ACK for every WRTE. Wait until the
         * peer's remaining send credit has been exhausted.
         */
        if(sync->pending_bytes < sync->their_asb)
            return ADB_ERR_OK;

        sync->pkt.payload_size =
                sizeof(sync->pending_bytes);

        res = adb__packet_write(
                sync->conn,
                &sync->pkt,
                &sync->pending_bytes);
        if(res != ADB_ERR_OK)
            return res;

        /* The ACK just replenished the peer's ASB.*/
        sync->their_asb += sync->pending_bytes;
        sync->pending_bytes = 0;
        return ADB_ERR_OK;
    }

    sync->pkt.payload_size = 0;
    res = adb__packet_write(
            sync->conn,
            &sync->pkt,
            NULL);
    if(res != ADB_ERR_OK)
        return res;

    return ADB_ERR_OK;
}

void adb__sync_close(
        adb__sync_t *sync)
{
    if(!sync)
        return;

    if(sync->opened)
    {
        sync->pkt.command = ADB__CMD_CLSE;
        sync->pkt.arg0 = sync->local_id;
        sync->pkt.arg1 = sync->remote_id;
        sync->pkt.payload_size = 0;

        (void)adb__packet_write(
                sync->conn,
                &sync->pkt,
                NULL);
    }

    adb__free(sync->pkt_payload);
    *sync = (adb__sync_t){0};
}
