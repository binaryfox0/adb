#ifndef ADB_DYNARR_H
#define ADB_DYNARR_H

#include <stddef.h>
#include <adb/adb_error.h>
#include "adb_compiler.h"

#define adb__dynarr_foreach(a, type, v) \
    for(size_t i = 0; i < (a)->size; i++, (v) = ((type*)(a)->data)[i])
#define adb__dynarr_foreach_rev(a, type, v) \
    for(size_t i = (a)->size; i-- > 0; (v) = ((type*)(a)->data)[i])
#define adb__dynarr_get(a, type, idx) ((((type*)(a)->data))[(idx)])

typedef struct 
{
    void   *data;
    size_t  size;
    size_t  capacity;
    size_t  elem_size;
} adb__dynarr_t;

adb_error_t adb__dynarr_init(
        adb__dynarr_t *a, 
        const size_t elem_size);

ADB__NODISCARD adb_error_t adb__dynarr_push(
        adb__dynarr_t *a, 
        const void *elem);

ADB__NODISCARD adb_error_t adb__dynarr_reserve(
        adb__dynarr_t *a, 
        const size_t capacity);

adb_error_t adb__dynarr_clear(
        adb__dynarr_t *a);

void adb__dynarr_destroy(
        adb__dynarr_t *a);

#endif
