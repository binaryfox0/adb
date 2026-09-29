#ifndef ADB_QUEUE_H
#define ADB_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct adb__queue adb__queue_t;

adb__queue_t *adb__queue_create(void);
void adb__queue_destroy(adb__queue_t *queue);

bool adb__queue_write(
        adb__queue_t *queue,
        const void *data,
        size_t size);

size_t adb__queue_read(
        adb__queue_t *queue,
        void *data,
        size_t size);

size_t adb__queue_peek(
        const adb__queue_t *queue,
        void *data,
        size_t size);

size_t adb__queue_size(const adb__queue_t *queue);
bool adb__queue_empty(const adb__queue_t *queue);

void adb__queue_clear(adb__queue_t *queue);

#endif
