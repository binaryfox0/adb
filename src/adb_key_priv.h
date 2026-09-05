#ifndef ADB_KEY_PRIV_H
#define ADB_KEY_PRIV_H

#include <stddef.h>
#include <adb/adb_error.h>

typedef struct adb_key adb_key_t;

int write_rsa2048_pkcs8_der( adb_key_t *key,
                             unsigned char *buf,
                             size_t buf_size,
                             unsigned char **out,
                             size_t *out_len );

#endif
