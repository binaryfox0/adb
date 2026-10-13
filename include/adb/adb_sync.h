#ifndef ADB_SYNC_H
#define ADB_SYNC_H

#include <stdint.h>
#include <adb/adb_error.h>
#include <adb/adb_conn.h>

#define ADB_REMOTE_PATH_MAX                   1024

typedef struct __attribute__((packed))
{
    uint32_t id;
    uint32_t error;
    uint64_t dev;
    uint64_t ino;
    uint32_t mode;
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    int64_t atime;
    int64_t mtime;
    int64_t ctime;
} adb_sync_stat_t;

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

adb_error_t adb_sync_stat(
        adb_conn_t *conn,
        const char *remote_path,
        adb_sync_stat_t *out,
        int *out_errno);

#endif
