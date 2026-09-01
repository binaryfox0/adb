#ifndef ADB_KEY_H
#define ADB_KEY_H

#include <adb/adb_error.h>

typedef struct adb_key adb_key_t;

adb_error_t adb_key_generate(
        adb_key_t **key);

void adb_key_destroy(adb_key_t *key);

#endif
