#include <adb/adb_stream.h>

#include "adb_stream_priv.h"
#include "adb_conn_priv.h"

#define ADB__OUR_ASB_CREDIT (32 * 1024 * 1024)

bool adb_stream_done(
		const adb_stream_t *stream)
{
	return stream ? stream->done : true;
}

adb_error_t adb_stream_error(
		const adb_stream_t *stream)
{
	return stream ? stream->error : ADB_ERR_PARAM;
}

adb_error_t adb__stream_ack(
        adb_stream_t *stream,
        const adb__packet_t *pkt)
{
    adb_error_t res = ADB_ERR_OK;
    adb__packet_t ack = {0};

    ack.command = ADB__CMD_OKAY;
    ack.arg0 = pkt->arg1;
    ack.arg1 = pkt->arg0;

    if(stream->delayed_ack)
    {
        stream->their_asb -= (int32_t)pkt->payload_size;
        stream->pending_ack_bytes += (int32_t)pkt->payload_size;
        if(stream->their_asb >= 0)
            return ADB_ERR_OK;

        ack.payload_size = sizeof(stream->pending_ack_bytes);
        res = adb__packet_write(stream->conn, &ack,
                &stream->pending_ack_bytes);
        if(res != ADB_ERR_OK)
            return res;

        stream->their_asb += stream->pending_ack_bytes;
        stream->pending_ack_bytes = 0;
        return ADB_ERR_OK;
    }

    return adb__packet_write(stream->conn, &ack, NULL);
}

adb_error_t adb__stream_open(
        adb_stream_t *stream,
        adb_conn_t *conn,
        const void *payload,
        const size_t payload_size)
{
    adb_error_t res = ADB_ERR_OK;
    adb__packet_t pkt = {0};

    if(!stream || !conn || !stream->packet_fn || stream->conn ||
            ((payload_size == 0) != (payload == NULL)) ||
            payload_size > UINT32_MAX)
        return ADB_ERR_PARAM;

    stream->delayed_ack = adb__conn_support(
            conn, ADB__FEATURE_DELAYED_ACK);
    stream->our_asb = stream->delayed_ack ?
            ADB__OUR_ASB_CREDIT : 0;
    stream->local_id = adb__conn_next_id(conn);

    pkt.command = ADB__CMD_OPEN;
    pkt.arg0 = stream->local_id;
    pkt.arg1 = (uint32_t)stream->our_asb;
    pkt.payload_size = (uint32_t)payload_size;

    res = adb__packet_write(conn, &pkt, payload);
    if(res != ADB_ERR_OK)
        return res;

    return adb__conn_stream_add(conn, stream);
}

void adb_stream_close(adb_stream_t *stream)
{
    if(!stream)
		return;
    if(stream->conn)
    {
        if(stream->opened && !stream->close_sent)
		{
			adb_error_t discarded = ADB_ERR_OK;
			adb__packet_t pkt = {0};

			pkt.command = ADB__CMD_CLSE;
			pkt.arg0 = stream->local_id;
			pkt.arg1 = stream->remote_id;
			discarded = adb__packet_write(stream->conn, &pkt, NULL);
			(void)discarded;
		}
        adb__conn_stream_remove(stream->conn, stream);
    }
    if(stream->destroy_fn)
        stream->destroy_fn(stream);
}