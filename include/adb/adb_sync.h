#ifndef ADB_SYNC_H
#define ADB_SYNC_H

#include <stdint.h>
#include <adb/adb_error.h>
#include <adb/adb_conn.h>

adb_error_t adb_sync_pull(
        adb_conn_t *conn,
        const char *remote_path,
        const adb_write_fn write_fn,
        void *userdata);

adb_error_t adb_sync_pull_file(
        adb_conn_t *conn,
        const char *remote_path,
        const char *local_path);

adb_error_t adb_sync_push(
        adb_conn_t *conn,
        const char *remote_path,
        const uint32_t mode,
        const uint32_t modified_time,
        const size_t size,
        const adb_read_fn read_fn,
        void *userdata);

adb_error_t adb_sync_push_file(
        adb_conn_t *conn,
        const char *local_path,
        const char *remote_path);

#endif
