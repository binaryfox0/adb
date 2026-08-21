#ifndef ADB_CONN_PRIV
#define ADB_CONN_PRIV

typedef struct adb_conn_info adb_conn_info_t;
void adb__conn_info_destroy(
        adb_conn_info_t *conn_info);

#endif