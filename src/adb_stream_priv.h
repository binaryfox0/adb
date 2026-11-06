#ifndef ADB_STREAM_PRIV_H
#define ADB_STREAM_PRIV_H

#include <stdbool.h>
#include <stdint.h>
#include <adb/adb_error.h>

#include "adb_packet.h"

typedef struct adb_conn adb_conn_t;
typedef struct adb_stream adb_stream_t;

typedef adb_error_t (*adb__stream_packet_fn)(
    adb_stream_t *stream,
    const adb__packet_t *pkt,
    uint8_t *payload);

typedef enum
{
    ADB__STREAM_SYNC_PULL,
    ADB__STREAM_SYNC_PUSH,
    ADB__STREAM_SYNC_LIST
} adb__stream_type_t;

typedef struct adb_stream
{
    bool done;
    bool opened;
    bool delayed_ack;
    bool send_ready;
    bool close_pending;
    bool close_sent;
    adb_error_t error;
    adb__stream_type_t type;
    adb_conn_t *conn;
    uint32_t local_id;
    uint32_t remote_id;
    int32_t their_asb;
    int32_t our_asb;
    int32_t pending_ack_bytes;
    adb_stream_t *next;
    void *opaque;
    adb__stream_packet_fn packet_fn;
    void (*destroy_fn)(adb_stream_t *stream);
} adb_stream_t;

ADB__NODISCARD adb_error_t adb__stream_ack(
        adb_stream_t *stream,
        const adb__packet_t *pkt);

ADB__NODISCARD adb_error_t adb__stream_open(
        adb_stream_t *stream,
        adb_conn_t *conn,
        const void *payload,
        size_t payload_size);

void adb_stream_close(adb_stream_t *stream);

#endif