#include "adb_transport.h"

adb_error_t adb__transport_read(
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

adb_error_t adb__transport_write(
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

void adb__transport_destroy(
        adb__transport_t *transport)
{
    if(!transport)
        return;

    if(transport->destroy)
        transport->destroy(transport->userdata);

    transport->userdata = NULL;
    transport->read = NULL;
    transport->write = NULL;
    transport->destroy = NULL;
}

