#ifndef ADB_STREAM_H
#define ADB_STREAM_H

#include <stdbool.h>
#include <adb/adb_error.h>

#define ADB_STREAM(x) ((adb_stream_t*)(x))

typedef struct adb_stream adb_stream_t;

bool adb_stream_done(const adb_stream_t *stream);

adb_error_t adb_stream_error(const adb_stream_t *stream);

#endif