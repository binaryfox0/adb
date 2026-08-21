#include <adb/adb_ctx.h>
#include "adb_ctx_priv.h"

#include <libusb.h>
#include "adb_alloc_priv.h"
#include "adb_log_priv.h"

adb_error_t adb_ctx_create(
        adb_ctx_t **ctx)
{
    adb_ctx_t *tmp = NULL;
    int res = 0;
    if(!ctx)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    res = libusb_init(&tmp->usb);
    if(res != 0)
    {
        ADB__ERROR("failed to create libusb context");
        ADB__INFO("reason: %s (%s)", 
                libusb_error_name(res), 
                libusb_strerror(res));
        adb__free(tmp);
        return ADB_ERR_USB;
    }

    *ctx = tmp;
    return ADB_ERR_OK;
}

void adb_ctx_destroy(
        adb_ctx_t *ctx)
{
    if(!ctx)
        return;

    for(size_t i = 0; i < ctx->infos_capacity; i++)
        adb__free(ctx->conn_infos[i]);
    adb__free(ctx->conn_infos);
    libusb_exit(ctx->usb);
    adb__free(ctx);
}