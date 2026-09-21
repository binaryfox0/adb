#include "adb_transport_custom.h"
#include "adb_transport.h"

#include "adb_alloc_priv.h"

typedef struct
{
    adb_read_fn read;
    adb_write_fn write;
    void *userdata;
} adb__custom_transport_t;

static adb_error_t adb__custom_read(
        void *userdata,
        void *buf,
        size_t size)
{
    adb__custom_transport_t *custom = userdata;
    int ret = 0;
    if(!custom || !custom->read)
        return ADB_ERR_PARAM;

    if(!buf && size)
        return ADB_ERR_PARAM;

    ret = custom->read(
            custom->userdata,
            buf,
            size);

    if(ret < 0)
        return (adb_error_t)(-ret);

    if(ret == 0)
        return ADB_ERR_DISCONNECTED;

    if((size_t)ret != size)
        return ADB_ERR_IO;

    return ADB_ERR_OK;
}


static adb_error_t adb__custom_write(
        void *userdata,
        const void *buf,
        size_t size)
{
    adb__custom_transport_t *custom = userdata;
    int ret = 0;
    if(!custom || !custom->write)
        return ADB_ERR_PARAM;

    if(!buf && size)
        return ADB_ERR_PARAM;

    ret = custom->write(
            custom->userdata,
            buf,
            size);

    if(ret < 0)
        return (adb_error_t)(-ret);

    if(ret == 0 && size != 0)
        return ADB_ERR_DISCONNECTED;

    if((size_t)ret != size)
        return ADB_ERR_IO;

    return ADB_ERR_OK;
}


static void adb__custom_destroy(
        void *userdata)
{
    adb__custom_transport_t *custom = userdata;
    if(!custom)
        return;

    adb__free(custom);
}


adb_error_t adb__custom_transport_create(
        adb__transport_t *transport,
        adb_read_fn read_cb,
        adb_write_fn write_cb,
        void *userdata)
{
    adb__custom_transport_t *custom = NULL;

    if(!transport || !read_cb || !write_cb)
        return ADB_ERR_PARAM;

    custom = adb__calloc(1, sizeof(*custom));
    if(!custom)
        return ADB_ERR_NO_MEM;

    custom->read = read_cb;
    custom->write = write_cb;
    custom->userdata = userdata;

    transport->userdata = custom;
    transport->read = adb__custom_read;
    transport->write = adb__custom_write;
    transport->destroy = adb__custom_destroy;

    return ADB_ERR_OK;
}
