#ifndef ADB_TRANSPORT_CUSTOM_H
#define ADB_TRANSPORT_CUSTOM_H

#include <adb/adb_error.h>
#include <adb/adb_conn.h>

typedef struct adb_wired_info adb_wired_info_t;
typedef struct adb__transport adb__transport_t;

adb_error_t adb__custom_transport_create(
        adb__transport_t *transport,
        adb_conn_read_fn read_fn,
        adb_conn_write_fn write_fn,
        void *userdata);

#endif
