#ifndef ADB_STR_H
#define ADB_STR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ADB__STR_NPOS ((size_t)-1)
#define ADB__STR_PRINTF_EXPAND(str) (int)(str)->len, (str)->ptr

typedef struct
{
    char *ptr;
    size_t len;
} adb__str_t;

static inline bool adb__str_is_empty(
        const adb__str_t *str) 
{
    return !str || !str->ptr || str->len == 0;
}

static inline bool adb__str_compare_cstr(
        const adb__str_t *str,
        const char *cstr)
{
    size_t i = 0;
    if (adb__str_is_empty(str))
        return cstr == NULL || cstr[0] == '\0';
    if (!cstr)
        return false;

    for (; i < str->len && cstr[i] != '\0'; ++i)
    {
        if (str->ptr[i] != cstr[i])
            return false;
    }

    return i == str->len && cstr[i] == '\0';
}

static inline bool adb__str_next_tok(
        adb__str_t *curr,
        const char delim,
        adb__str_t *out)
{
    size_t index = 0;

    if(adb__str_is_empty(curr))
        return false;

    if(!curr->ptr)
        return false;

    while(index < curr->len && curr->ptr[index] != delim)
        index++;

    if(out)
    {
        out->ptr = curr->ptr;
        out->len = index;
    }

    if(index < curr->len) {
        curr->ptr += index + 1;
        curr->len -= index + 1;
    } else {
        curr->ptr += index;
        curr->len -= index;
    }

    return true;
}

static inline size_t adb__str_find_char(
        const adb__str_t *str,
        const char ch)
{
    if(adb__str_is_empty(str))
        return ADB__STR_NPOS;
    
    for(size_t i = 0; i < str->len; i++)
    {
        if(str->ptr[i] == ch)
            return i;
    }

    return ADB__STR_NPOS;
}

#endif
