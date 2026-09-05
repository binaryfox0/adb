#ifndef ADB_TRANSPORT_H
#define ADB_TRANSPORT_H

#include <stddef.h>
#include <adb/adb_error.h>

typedef adb_error_t (*adb__transport_read_fn)(
        void *userdata,
        void *buf,
        const size_t size);

typedef adb_error_t (*adb__transport_write_fn)(
        void *userdata,
        const void *buf,
        const size_t size);

typedef void (*adb__transport_destroy_fn)(
        void *userdata);

typedef struct adb__transport
{
    void *userdata;

    adb__transport_read_fn read;
    adb__transport_write_fn write;
    adb__transport_destroy_fn destroy;
} adb__transport_t;

adb_error_t adb__transport_read(
        adb__transport_t *transport,
        void *buf,
        size_t size);

adb_error_t adb__transport_write(
        adb__transport_t *transport,
        const void *buf,
        size_t size);

void adb__transport_destroy(
        adb__transport_t *transport);

#endif
