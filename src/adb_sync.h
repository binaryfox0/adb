#ifndef ADB_SYNC_H
#define ADB_SYNC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "adb_compiler.h"
#include "adb_conn_priv.h"
#include "adb_packet.h"

typedef struct adb__sync 
{
    adb_conn_t *conn;
    bool delayed_ack;

    uint32_t local_id;
    uint32_t remote_id;

    adb__packet_t pkt;
    uint8_t *pkt_payload;
    int32_t their_asb;
    int32_t pending_bytes;
    int32_t our_asb;
    bool opened;
    bool send_ready;
} adb__sync_t;

ADB__NODISCARD adb_error_t adb__sync_open(
        adb__sync_t *sync,
        adb_conn_t *conn);

ADB__NODISCARD adb_error_t adb__sync_write(
        adb__sync_t *sync,
        const void *payload,
        size_t payload_size);

ADB__NODISCARD adb_error_t adb__sync_read(
        adb__sync_t *sync);

adb_error_t adb__sync_handle_okay(
        adb__sync_t *sync);

adb_error_t adb__sync_ack(
        adb__sync_t *sync);

void adb__sync_close(
        adb__sync_t *sync);

#endif
