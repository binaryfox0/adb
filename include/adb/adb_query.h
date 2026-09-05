#ifndef ADB_QUERY_H
#define ADB_QUERY_H

#include <stddef.h>
#include <adb/adb_error.h>

typedef struct adb_ctx adb_ctx_t;
typedef struct adb_wired_info adb_wired_info_t;
typedef struct adb_wireless_info adb_wireless_info_t;

adb_error_t adb_query_wired(
        adb_ctx_t *ctx,
        adb_wired_info_t ***infos,
        size_t *info_count);

const char *adb_wired_info_manufacturer(
        const adb_wired_info_t *info);

const char *adb_wired_info_product(
        const adb_wired_info_t *info);

adb_error_t adb_query_wireless(
        adb_ctx_t *ctx,
        adb_wireless_info_t ***infos,
        size_t *info_count);
         
#endif
