#include <adb/adb_alloc.h>
#include "adb_alloc_priv.h"

#include <stdlib.h>
#include <string.h>

static void *adb__malloc_default(
        void* userdata, 
        size_t size);
static void *adb__realloc_default(
        void* userdata,
        void *p, 
        size_t size);
static void adb__free_default(
        void* userdata, 
        void *p);

static adb_alloc_t adb__alloc =
{
    .malloc = adb__malloc_default,
    .realloc = adb__realloc_default,
    .free = adb__free_default,
    .userdata = NULL
};

static void *adb__malloc_default(
        void* userdata, 
        size_t size)
{
    (void)userdata;
    return malloc(size);
}

static void *adb__realloc_default(
        void* userdata,
        void *p, 
        size_t size)
{
    (void)userdata;
    return realloc(p, size);
}

static void adb__free_default(
        void* userdata, 
        void *p)
{
    (void)userdata;
    free(p);
}

void adb_set_alloc(
        const adb_alloc_t *alloc)
{
    if(!alloc)
    {
        adb__alloc.malloc = adb__malloc_default;
        adb__alloc.realloc = adb__realloc_default;
        adb__alloc.free = adb__free_default;
        adb__alloc.userdata = NULL;
        return;
    }

    adb__alloc = *alloc;
}

void *adb__malloc(
        size_t size) {
    return adb__alloc.malloc(adb__alloc.userdata, size);
}

void *adb__calloc(
        size_t nmemb, 
        size_t size)
{
    void *p = adb__alloc.malloc(adb__alloc.userdata, nmemb * size);
    memset(p, 0, nmemb * size);
    return p;
}

void *adb__realloc(
        void *p, 
        size_t size) {
    return adb__alloc.realloc(adb__alloc.userdata, p, size);
}

void adb__free(
        void *p) {
    adb__alloc.free(adb__alloc.userdata, p);
}