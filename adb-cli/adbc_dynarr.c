#include "adbc_dynarr.h"

#include <stdlib.h>
#include <string.h>

bool adbc_dynarr_init(
        adbc_dynarr_t *a, 
        const size_t elem_size)
{
    if (!a || elem_size == 0)
        return false;

    *a = (adbc_dynarr_t){0};
    a->elem_size = elem_size;
    return true;
}

static bool adbc_dynarr_reserve(
        adbc_dynarr_t *a,
        const size_t capacity)
{
    void *new_data = NULL;
    size_t old_capacity;

    if (!a || a->elem_size == 0)
        return false;
    if (capacity <= a->capacity)
        return false;
    if (capacity > SIZE_MAX / a->elem_size)
        return false;

    old_capacity = a->capacity;
    new_data = realloc(
        a->data,
        capacity * a->elem_size
    );

    if (!new_data)
        return false;

    a->data = new_data;
    a->capacity = capacity;

    memset((uint8_t*)a->data + old_capacity * a->elem_size,
        0, (capacity - old_capacity) * a->elem_size
    );

    return true;
}

bool adbc_dynarr_push(
        adbc_dynarr_t *a, 
        const void *elem)
{
    if (!a)
        return false;

    if(a->size == a->capacity) 
    {
        size_t new_cap = a->capacity ? a->capacity * 2 : 8;

        /* integer overflow check */
        if(new_cap < a->capacity)
            return false;
        if(!adbc_dynarr_reserve(a, new_cap))
            return false;
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
    return true;
}

void adbc_dynarr_destroy(adbc_dynarr_t *a)
{
    if (!a)
        return;

    free(a->data);
    *a = (adbc_dynarr_t){0};
}
