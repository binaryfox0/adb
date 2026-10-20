#ifndef ADB_ALLOC_H
#define ADB_ALLOC_H

#include <stddef.h>
#include <adb/adb_error.h>

typedef void *(*adb_malloc_fn)(void *userdata, size_t size);
typedef void *(*adb_realloc_fn)(void *userdata, void *p, size_t size);
typedef void (*adb_free_fn)(void *userdata, void *p);

typedef struct
{
    adb_malloc_fn malloc;
    adb_realloc_fn realloc;
    adb_free_fn free;
    void *userdata;
} adb_alloc_t;

adb_error_t adb_set_alloc(
        const adb_alloc_t *alloc);

#endif
