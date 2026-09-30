#ifndef ADB_QUEUE_H
#define ADB_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <adb/adb_error.h>
#include "adb_compiler.h"

typedef struct
{
    uint8_t *data;
    size_t capacity;
    size_t offset;
    size_t size;
} adb__queue_t;

void adb__queue_destroy(adb__queue_t *queue);

/*
 * Push bytes into the queue
 */
ADB__NODISCARD adb_error_t adb__queue_push(
        adb__queue_t *queue,
        const void *data,
        const size_t size);

/*
 * Pop bytes from the queue into data,
 * limited to the requested size or queue size, whichever is smaller.
 */
adb_error_t adb__queue_pop(
        adb__queue_t *queue,
        void *data,
        const size_t size);

#endif
