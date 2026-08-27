#ifndef ADB_CONN_PRIV
#define ADB_CONN_PRIV

typedef struct adb_usb_info adb_usb_info_t;
void adb__conn_info_destroy(
        adb_usb_info_t *conn_info);

#endif
