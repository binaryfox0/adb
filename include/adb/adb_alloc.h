#ifndef ADB_ALLOC_H
#define ADB_ALLOC_H

#include <stddef.h>

typedef struct
{
    void *(*malloc)(void*, size_t);
    void *(*realloc)(void*, void*, size_t);
    void (*free)(void*, void*);
    void *userdata;
} adb_alloc_t;

void adb_set_alloc(
        const adb_alloc_t *alloc);

#endif
