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
    psa_status_t status = PSA_SUCCESS;
    int err = 0;
    if(!ctx)
        return ADB_ERR_PARAM;

    ADB__INFO("creating new library context");
    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;


    status = psa_crypto_init();
    if(status != PSA_SUCCESS)
    {
        ADB__ERROR("failed to create random generator");
        ADB__INFO("tfpsacrypto status: %d", (int)status);
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
    
    adb__dynarr_init(&tmp->wired_infos, 
            sizeof(adb_wired_info_t*));
    adb__dynarr_init(&tmp->wireless_infos, 
            sizeof(adb_wireless_info_t*));
    adb__dynarr_init(&tmp->dev_winfos, 
            sizeof(adb_wireless_info_t*));
    adb__dynarr_init(&tmp->pair_winfos, 
            sizeof(adb_wireless_info_t*));
    *ctx = tmp;
    ADB__INFO("created library context successfully");
    return ADB_ERR_OK;
}

void adb_ctx_destroy(
        adb_ctx_t *ctx)
{
    adb_wired_info_t *wired_info = NULL;
    adb_wireless_info_t *wireless_info = NULL;
    if(!ctx)
        return;

    adb__dynarr_foreach(&ctx->wired_infos, adb_wired_info_t*, wired_info)
        adb__wired_info_destroy(wired_info);
    adb__dynarr_foreach(&ctx->wireless_infos, adb_wireless_info_t*, wireless_info)
        adb__wireless_info_destroy(wireless_info);
    adb__dynarr_foreach(&ctx->dev_winfos, adb_wireless_info_t*, wireless_info)
        adb__wireless_info_destroy(wireless_info);
    adb__dynarr_foreach(&ctx->pair_winfos, adb_wireless_info_t*, wireless_info)
        adb__wireless_info_destroy(wireless_info);

    adb__dynarr_destroy(&ctx->wired_infos);
    adb__dynarr_destroy(&ctx->wireless_infos);
    adb__dynarr_destroy(&ctx->dev_winfos);
    adb__dynarr_destroy(&ctx->pair_winfos);

    if((ctx->features & ADB__FEATURE_WIRED) != 0)
        libusb_exit(ctx->usb);
    adb__free(ctx);
}
