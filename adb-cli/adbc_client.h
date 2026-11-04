#ifndef ADBC_CLI_PRIV_H
#define ADBC_CLI_PRIV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <aparse.h>
#include <adb/adb_log.h>

typedef struct
{
    adb_log_level_t opt_lvl;
    const char *opt_key;
    const char *opt_serial;
    aparse_list push_local_paths;
    union 
    {
        struct 
        {
            const char *local_path;
            const char *remote_path;
        };
        struct
        {
            const char *endpoint;
            const char *code;
        };
        const char *serial;
    };
    aparse_list pull_remote_paths;
    const char *pull_local_path;
    const char *stat_path;
    const char *ls_path;
} adbc_cmd_options_t;

void kill_server_cmd(void *data);
void devices_cmd(void *data);
void pair_cmd(void *data);
void pair_qr_cmd(void *data);
void connect_cmd(void *data);
void disconnect_cmd(void *data);
void push_cmd(void *data);
void pull_cmd(void *data);
void stat_cmd(void *data);
void ls_cmd(void *data);
#endif
