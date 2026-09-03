#ifndef ADB_CONN_PRIV_H
#define ADB_CONN_PRIV_H

typedef struct adb_wired_info adb_usb_info_t;
void adb__conn_info_destroy(
        adb_usb_info_t *conn_info);

#endif
