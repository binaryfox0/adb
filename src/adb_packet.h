#ifndef ADB_MESSAGE_H
#define ADB_MESSAGE_H

#include <stdint.h>
#include <adb/adb_error.h>

#define ADB__CMD(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#define ADB__CMD_CNXN ADB__CMD('C', 'N', 'X', 'N')

typedef struct adb_conn adb_conn_t;

typedef struct {
    uint32_t command;      /* command identifier constant      */
    uint32_t arg0;         /* first argument                   */
    uint32_t arg1;         /* second argument                  */
    uint32_t payload_size; /* payload size (0 is allowed) */
    uint32_t data_check;   /* checksum of payload         */
    uint32_t magic;        /* command ^ 0xffffffff             */
    const void *payload;
} adb__packet_t;

adb_error_t adb__packet_send(
        adb_conn_t *conn,
        adb__packet_t *pkt);

#endif
