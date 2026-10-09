#include <adb/adb_query.h>
#include "adb_query_priv.h"

#include <stdio.h>
#include <string.h>

#include "adb_ctx_priv.h"
#include "adb_log_priv.h"

adb_error_t adb_query_wireless(
        adb_ctx_t *ctx,
        adb_wireless_info_t ***infos,
        size_t *info_count)
{
    adb_error_t res = ADB_ERR_OK;

    if(!infos || !info_count)
        return ADB_ERR_PARAM;

    *infos = NULL;
    *info_count = 0;

    if(!ctx)
        return ADB_ERR_PARAM;

    res = adb__query_mdns(
            "_adb-tls-connect._tcp.local.",
            sizeof("_adb-tls-connect._tcp.local.") - 1,
            false,
            &ctx->wireless_infos,
            &ctx->wireless_info_count);
    if(res != ADB_ERR_OK)
        return res;

    *infos = ctx->wireless_infos.data;
    *info_count = ctx->wireless_info_count;
    return ADB_ERR_OK;
}

adb_error_t adb_find_wireless(
        adb_ctx_t *ctx,
        const char *guid,
        adb_wireless_info_t **out_info)
{
    adb_error_t res = ADB_ERR_OK;
    int written = 0;
    char query_name[ADB__MDNS_NAME_LENGTH_MAX] = {0};

    if(!out_info)
        return ADB_ERR_PARAM;

    *out_info = NULL;

    if(!ctx || !guid || guid[0] == '\0')
        return ADB_ERR_PARAM;

    written = snprintf(query_name, sizeof(query_name),
            "%s._adb-tls-connect._tcp.local.", guid);
    if(written < 0 || (size_t)written >= sizeof(query_name))
    {
        ADB__ERROR("device GUID was too long to fit into buffer");
        ADB__INFO("GUID length: %zu bytes", strlen(guid));
        return ADB_ERR_GENERIC;
    }

    res = adb__query_mdns(
            query_name,
            (size_t)written,
            true,
            &ctx->dev_winfos,
            &ctx->dev_winfo_count);
    if(res != ADB_ERR_OK)
        return res;

    if(ctx->dev_winfo_count == 0)
        return ADB_ERR_NOT_FOUND;
    if(ctx->dev_winfo_count != 1)
        return ADB_ERR_AMBIGUOUS;

    *out_info = adb__dynarr_get(
            &ctx->dev_winfos, adb_wireless_info_t *, 0);
    return *out_info ? ADB_ERR_OK : ADB_ERR_GENERIC;
}

adb_error_t adb_find_wireless_pairing(
        adb_ctx_t *ctx,
        const char *service_name,
        adb_wireless_info_t **out_info)
{
    adb_error_t res = ADB_ERR_OK;
    int written = 0;
    char query_name[ADB__MDNS_NAME_LENGTH_MAX] = {0};

    if(!out_info)
        return ADB_ERR_PARAM;

    *out_info = NULL;

    if(!ctx || !service_name || service_name[0] == '\0')
        return ADB_ERR_PARAM;

    written = snprintf(query_name, sizeof(query_name),
            "%s._adb-tls-pairing._tcp.local.", service_name);
    if(written < 0 || (size_t)written >= sizeof(query_name))
    {
        ADB__ERROR("service name was too long to fit into buffer");
        ADB__INFO("service name length: %zu bytes",
                strlen(service_name));
        return ADB_ERR_GENERIC;
    }

    res = adb__query_mdns(
            query_name,
            (size_t)written,
            true,
            &ctx->pair_winfos,
            &ctx->pair_winfo_count);
    if(res != ADB_ERR_OK)
        return res;

    if(ctx->pair_winfo_count == 0)
        return ADB_ERR_NOT_FOUND;
    if(ctx->pair_winfo_count != 1)
        return ADB_ERR_AMBIGUOUS;

    *out_info = adb__dynarr_get(
            &ctx->pair_winfos, adb_wireless_info_t *, 0);
    return *out_info ? ADB_ERR_OK : ADB_ERR_GENERIC;
}
