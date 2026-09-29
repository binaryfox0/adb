#include "adb_alloc_priv.h"

#include <stdlib.h>
#include <string.h>

#include "adb_queue.h"

#define ADB_QUEUE_INITIAL_CAPACITY 4096U

struct adb__queue
{
    uint8_t *data;
    size_t capacity;
    size_t offset;
    size_t size;
};

static bool adb__queue_reserve(
        adb__queue_t *queue,
        const size_t size)
{
    size_t new_capacity = 0;
    uint8_t *new_data = NULL;

    if(size <= queue->capacity - queue->size)
        return true;

    if(queue->offset != 0)
    {
        memmove(queue->data,
                queue->data + queue->offset,
                queue->size);
        queue->offset = 0;
        if(size <= queue->capacity - queue->size)
            return true;
    }

    new_capacity = queue->capacity;
    if(new_capacity == 0)
        new_capacity = ADB_QUEUE_INITIAL_CAPACITY;

    while(new_capacity - queue->size < size)
    {
        if(new_capacity > SIZE_MAX / 2U)
        {
            new_capacity = queue->size + size;
            break;
        }

        new_capacity *= 2U;
    }

    new_data = adb__realloc(queue->data, new_capacity);
    if(!new_data)
        return false;

    queue->data = new_data;
    queue->capacity = new_capacity;
    return true;
}

adb__queue_t *adb__queue_create(void) {
    return adb__calloc(1, sizeof(adb__queue_t));
}

void adb__queue_destroy(adb__queue_t *queue)
{
    if(!queue)
        return;

    free(queue->data);
    free(queue);
}

bool adb__queue_write(
        adb__queue_t *queue,
        const void *data,
        size_t size)
{
    if(!queue || (!data && size != 0))
        return false;

    if(size == 0)
        return true;

    if(!adb__queue_reserve(queue, size))
        return false;

    memcpy(queue->data + queue->offset + queue->size,
            data, size);
    queue->size += size;
    return true;
}

size_t adb__queue_read(
        adb__queue_t *queue,
        void *data,
        size_t size)
{
    size_t read_size = 0;
    if(!queue || (!data && size != 0))
        return 0;

    if(size == 0 || queue->size == 0)
        return 0;

    read_size = size;

    if(read_size > queue->size)
        read_size = queue->size;

    memcpy(
            data,
            queue->data + queue->offset,
            read_size);

    queue->offset += read_size;
    queue->size -= read_size;

    if(queue->size == 0)
        queue->offset = 0;

    return read_size;
}

size_t adb__queue_size(
        const adb__queue_t *queue) {
    return queue ? queue->size : 0;
}

bool adb__queue_empty(
        const adb__queue_t *queue) {
    return queue ? queue->size == 0 : true;
}

void adb__queue_clear(
        adb__queue_t *queue)
{
    if(!queue)
        return;

    queue->offset = 0;
    queue->size = 0;
}
