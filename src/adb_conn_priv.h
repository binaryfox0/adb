#ifndef ADB_CONN_PRIV_H
#define ADB_CONN_PRIV_H

#include <stddef.h>
#include <adb/adb_error.h>

typedef struct adb_conn adb_conn_t;

adb_error_t adb__conn_read(
        adb_conn_t *conn,
        void *buf,
        const size_t size);


adb_error_t adb__conn_write(
        adb_conn_t *conn,
        const void *buf,
        const size_t size);

#endif
