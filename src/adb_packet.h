#ifndef ADB_MESSAGE_H
#define ADB_MESSAGE_H

#include <stdint.h>

#define ADB__COMMAND(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#define ADB__CMD_CNXN ADB__COMMAND('C', 'N', 'X', 'N')

typedef struct {
    uint32_t command;     /* command identifier constant      */
    uint32_t arg0;        /* first argument                   */
    uint32_t arg1;        /* second argument                  */
    uint32_t data_length; /* length of payload (0 is allowed) */
    uint32_t data_check;  /* checksum of data payload         */
    uint32_t magic;       /* command ^ 0xffffffff             */
} adb__message_t;

typedef struct {
    adb__message_t msg;
    const void *payload;
} adb__packet_t;

#endif