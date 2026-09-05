#include <adb/adb_ctx.h>
#include "adb_ctx_priv.h"

#include <libusb.h>
#include "adb_alloc_priv.h"
#include "adb_log_priv.h"
#include "adb_conn_priv.h"
#include "adb_query_priv.h"

adb_error_t adb_ctx_create(
        adb_ctx_t **ctx)
{
    adb_ctx_t *tmp = NULL;
    int err = 0;
    if(!ctx)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    mbedtls_entropy_init(&tmp->entropy);
    mbedtls_ctr_drbg_init(&tmp->drbg);

    err = mbedtls_ctr_drbg_seed(
            &tmp->drbg,
            mbedtls_entropy_func,
            &tmp->entropy,
            NULL,
            0);
    if(err != 0)
    {
        adb__log_err_mbedtls(err, "failed to create random generator");
        adb_ctx_destroy(tmp);
        return ADB_ERR_CRYPTO;
    }

    err = libusb_init(&tmp->usb);
    if(err != 0)
    {
        ADB__ERROR("failed to create libusb context");
        ADB__INFO("reason: %s (%s)", 
                libusb_error_name(err), 
                libusb_strerror(err));
        ADB__WARN("wired-related features will be unavailable");
    } else
        tmp->features |= ADB__FEATURE_WIRED;

    tmp->features |= ADB__FEATURE_WIRELESS;

    if(tmp->features == 0)
    {
        ADB__ERROR("failed to create at least one feature");
        adb_ctx_destroy(tmp);
        return ADB_ERR_UNSUPPORTED; // should be GENERIC?
    }
    
    *ctx = tmp;
    return ADB_ERR_OK;
}

void adb_ctx_destroy(
        adb_ctx_t *ctx)
{
    if(!ctx)
        return;
    
    mbedtls_entropy_free(&ctx->entropy);
    mbedtls_ctr_drbg_free(&ctx->drbg);

    for(size_t i = 0; i < ctx->infos_capacity; i++)
        adb__wired_info_destroy(ctx->wired_infos[i]);
    adb_free(ctx->wired_infos);
    if((ctx->features & ADB__FEATURE_WIRED) != 0)
        libusb_exit(ctx->usb);
    adb_free(ctx);
}
