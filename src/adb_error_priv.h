#ifndef ADB_ERROR_PRIV_H
#define ADB_ERROR_PRIV_H

#include <adb/adb_error.h>

adb_error_t adb__error_from_errno(
        const int error);
adb_error_t adb__error_from_libusb(
        const int error);

#endif
