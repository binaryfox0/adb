#include "adb_dynarr.h"

#include <string.h>
#include "adb_alloc_priv.h"

adb_error_t adb__dynarr_init(
        adb__dynarr_t *a, 
        const size_t elem_size)
{
    if (!a || elem_size == 0)
        return ADB_ERR_PARAM;

    *a = (adb__dynarr_t){0};
    a->elem_size = elem_size;
    return ADB_ERR_OK;
}

adb_error_t adb__dynarr_reserve(
        adb__dynarr_t *a,
        const size_t capacity)
{
    void *new_data = NULL;
    size_t old_capacity;

    if (!a)
        return ADB_ERR_PARAM;
    if (capacity <= a->capacity)
        return ADB_ERR_OK;
    if (capacity > SIZE_MAX / a->elem_size)
        return ADB_ERR_TOO_LONG;

    old_capacity = a->capacity;
    new_data = adb__realloc(
        a->data,
        capacity * a->elem_size
    );

    if (!new_data)
        return ADB_ERR_NO_MEM;

    a->data = new_data;
    a->capacity = capacity;

    memset((uint8_t*)a->data + old_capacity * a->elem_size,
        0, (capacity - old_capacity) * a->elem_size
    );

    return ADB_ERR_OK;
}

adb_error_t adb__dynarr_push(
        adb__dynarr_t *a, 
        const void *elem)
{
    if (!a)
        return ADB_ERR_PARAM;

    if (a->size == a->capacity) 
    {
        size_t new_cap = a->capacity ? a->capacity * 2 : 8;

        /* integer overflow check */
        if (new_cap < a->capacity)
            return ADB_ERR_TOO_LONG;
        if(adb__dynarr_reserve(a, new_cap) != ADB_ERR_OK)
            return ADB_ERR_NO_MEM;
    }

    if(elem)
    {
        memcpy((uint8_t*)a->data + a->size * a->elem_size,
                elem, a->elem_size);
    } else {
        memset((uint8_t*)a->data + a->size * a->elem_size, 
                0, a->elem_size);
    }

    a->size++;
    return ADB_ERR_OK;
}

adb_error_t adb__dynarr_clear(
        adb__dynarr_t *a)
{
    if(!a)
        return ADB_ERR_PARAM;
    a->size = 0;
    return ADB_ERR_OK;
}

void adb__dynarr_destroy(adb__dynarr_t *a)
{
    if (!a)
        return;

    adb__free(a->data);
    *a = (adb__dynarr_t){0};
}
