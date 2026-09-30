#ifndef ADB_TRANSPORT_H
#define ADB_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <adb/adb_error.h>
#include "adb_compiler.h"

typedef adb_error_t (*adb__transport_read_fn)(
        void *userdata,
        void *buf,
        const size_t size);

typedef adb_error_t (*adb__transport_read_timeout_fn)(
        void *userdata,
        void *buf,
        const size_t size,
        const uint32_t timeout);

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
    adb__transport_read_timeout_fn read_timeout;
    adb__transport_write_fn write;
    adb__transport_destroy_fn destroy;
} adb__transport_t;

ADB__NODISCARD ADB__INLINE adb_error_t adb__transport_read(
        adb__transport_t *transport,
        void *buf,
        size_t size)
{
    if(!transport || !transport->read)
        return ADB_ERR_PARAM;

    return transport->read(
            transport->userdata,
            buf,
            size);
}

ADB__NODISCARD ADB__INLINE adb_error_t adb__transport_read_timeout(
        adb__transport_t *transport,
        void *buf,
        const size_t size,
        const uint32_t timeout_ms)
{
    if(!transport || !transport->read)
        return ADB_ERR_PARAM;

    return transport->read_timeout(
            transport->userdata,
            buf, size,
            timeout_ms);
}

ADB__NODISCARD ADB__INLINE adb_error_t adb__transport_write(
        adb__transport_t *transport,
        const void *buf,
        size_t size)
{
    if(!transport || !transport->write)
        return ADB_ERR_PARAM;

    return transport->write(
            transport->userdata,
            buf,
            size);
}

ADB__INLINE void adb__transport_destroy(
        adb__transport_t *transport)
{
    if(!transport)
        return;

    if(transport->destroy)
        transport->destroy(transport->userdata);
    *transport = (adb__transport_t){0};
}

#endif
