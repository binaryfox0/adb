#ifndef ADB_ALLOC_PRIV_H
#define ADB_ALLOC_PRIV_H

#include <stddef.h>
#include <adb/adb_alloc.h>

const adb_alloc_t *adb__alloc_get(void);

void *adb__malloc(
        size_t size);

void *adb__calloc(
        size_t nmemb, 
        size_t size);

void *adb__realloc(
        void *p, 
        size_t size);

void adb__free(
        void *p);

#endif
