#ifndef ADB_ALLOC_PRIV_H
#define ADB_ALLOC_PRIV_H

#include <stddef.h>

void *adb__malloc(
        size_t size);

void *adb__calloc(
        size_t nmemb, 
        size_t size);

void *adb__realloc(
        void *p, 
        size_t size);

void adb_free(
        void *p);

#endif
