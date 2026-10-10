#ifndef ADBC_CLI_PRIV_H
#define ADBC_CLI_PRIV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <adb/adb_log.h>

typedef struct
{
    adb_log_level_t level;
    const char *local_path;
    const char *remote_path;
} adbc_cmd_options_t;

void kill_server_cmd(void *data);
void devices_cmd(void *data);
void pair_qr_cmd(void *data);

#endif
