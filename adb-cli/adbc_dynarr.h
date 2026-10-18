#ifndef ADBC_DYNARR_H
#define ADBC_DYNARR_H

#include <stddef.h>
#include <stdbool.h>

#define adbc_dynarr_foreach(a, type, v) \
    for (size_t i = 0; \
         i < (a)->size && (((v) = ((type *)(a)->data)[i]), 1); \
         i++)
#define adbc_dynarr_foreach_rev(a, type, v) \
    for (size_t i = (a)->size; \
         i-- > 0 && (((v) = ((type *)(a)->data)[i]), 1); )
#define adbc_dynarr_get(a, type, idx) \
    (((type *)(a)->data)[(idx)])

typedef struct 
{
    void   *data;
    size_t  size;
    size_t  capacity;
    size_t  elem_size;
} adbc_dynarr_t;

bool adbc_dynarr_init(
        adbc_dynarr_t *a, 
        const size_t elem_size);

bool adbc_dynarr_push(
        adbc_dynarr_t *a, 
        const void *elem);

void adbc_dynarr_destroy(
        adbc_dynarr_t *a);

#endif
