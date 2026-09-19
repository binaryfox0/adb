#ifndef ADB_MESSAGE_H
#define ADB_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <adb/adb_error.h>

#define ADB__PACKET_MAX_SUPPORTED_VER 0x01000001
#define ADB__PACKET_MAX_PAYLOAD_SIZE (1024 * 1024)

#define ADB__CMD_ENCODE(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#define ADB__CMD_CNXN ADB__CMD_ENCODE('C', 'N', 'X', 'N')
#define ADB__CMD_AUTH ADB__CMD_ENCODE('A', 'U', 'T', 'H')
#define ADB__CMD_STLS ADB__CMD_ENCODE('S', 'T', 'L', 'S')
#define ADB__CMD_OPEN ADB__CMD_ENCODE('O', 'P', 'E', 'N')
#define ADB__CMD_WRTE ADB__CMD_ENCODE('W', 'R', 'T', 'E')
#define ADB__CMD_OKAY ADB__CMD_ENCODE('O', 'K', 'A', 'Y')
#define ADB__CMD_CLSE ADB__CMD_ENCODE('C', 'L', 'S', 'E')

#define ADB__STLS_VERSION       0x01000000
#define ADB__STLS_MIN_VERSION   0x01000000

typedef struct adb_conn adb_conn_t;

typedef enum 
{
    ADB__AUTH_TOKEN,
    ADB__AUTH_SIGNATURE,
    ADB__AUTH_PUBLIC_KEY
} adb__auth_type_t;

typedef struct 
{
    uint32_t command;      /* command identifier constant      */
    uint32_t arg0;         /* first argument                   */
    uint32_t arg1;         /* second argument                  */
    uint32_t payload_size; /* payload size (0 is allowed) */
} adb__packet_t;

adb_error_t adb__packet_write(
        adb_conn_t *conn,
        const adb__packet_t *pkt,
        const void *payload);

/*
 * Read a packet and allocate a buffer to store the payload
 */
adb_error_t adb__packet_read(
        adb_conn_t *conn,
        adb__packet_t *out_pkt,
        void **out_payload);

/*
 * Read a packet and store into an existing buffer
 */
adb_error_t adb__packet_read_into(
        adb_conn_t *conn,
        adb__packet_t *out_pkt,
        void *out_payload,
        const size_t max_size);

bool adb__packet_check_cmd(
        adb__packet_t *pkt,
        const uint32_t expected);

#endif
