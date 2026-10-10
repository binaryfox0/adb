#include "adbc_io.h"

#include <errno.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <yyjson.h>

#include "adbc_log.h"

#define ADBC__MAX_JSON_SIZE (8U * 1024U)
#ifdef MSG_NOSIGNAL
#   define ADBC__SEND_FLAGS MSG_NOSIGNAL
#else
#   define ADBC__SEND_FLAGS 0
#endif


static bool adbc__recv_all(
        const int fd,
        void *data, 
        const size_t size)
{
    uint8_t *bytes = data;
    size_t offset = 0U;

    if(fd < 0 || (size > 0U && data == NULL))
        return false;

    while(offset < size)
    {
        ssize_t result = recv(
                fd, 
                bytes + offset, 
                size - offset, 
                0);

        if(result < 0)
        {
            if(errno == EINTR)
                continue;
            adbc_log_err_errno("failed to receive data");
            return false;
        }

        if(result == 0)
            return false;

        offset += (size_t)result;
    }

    return true;
}

bool adbc_recv_json(
        const int fd, 
        yyjson_doc **out_doc)
{
    uint16_t length_be = 0U;
    size_t length = 0U;
    char *json = NULL;
    yyjson_read_err read_err = {0};
    yyjson_doc *doc = NULL;

    if(out_doc != NULL)
        *out_doc = NULL;

    if(fd < 0 || out_doc == NULL)
        return false;

    if(!adbc__recv_all(fd, &length_be, sizeof(length_be)))
        return false;

    length = (size_t)ntohs(length_be);
    if(length == 0U || length > ADBC__MAX_JSON_SIZE)
    {
        ADBC_ERROR("invalid JSON frame length: %zu", length);
        return false;
    }

    json = malloc(length + 1U);
    if(json == NULL)
    {
        ADBC_ERROR("failed to allocate JSON receive buffer");
        return false;
    }

    if(!adbc__recv_all(fd, json, length))
    {
        free(json);
        return false;
    }

    json[length] = '\0';
    doc = yyjson_read_opts(json, length, 0, NULL, &read_err);
    if(doc == NULL)
    {
        ADBC_ERROR("failed to parse JSON payload");
        ADBC_INFO("reason: %s, at %zu byte",
                read_err.msg != NULL ? read_err.msg : "unknown ADBC_ERROR",
                read_err.pos);
        free(json);
        return false;
    }

    *out_doc = doc;
    free(json);
    return true;
}

static bool adbc__send_all(const int fd, const void *data, const size_t size)
{
    const uint8_t *bytes = data;
    size_t offset = 0U;

    if(fd < 0 || (size > 0U && data == NULL))
        return false;

    while(offset < size)
    {
        ssize_t result = send(fd, bytes + offset, size - offset,
                ADBC__SEND_FLAGS);

        if(result < 0)
        {
            if(errno == EINTR)
                continue;
            adbc_log_err_errno("failed to send data");
            return false;
        }

        if(result == 0)
        {
            ADBC_ERROR("socket closed while sending data");
            return false;
        }

        offset += (size_t)result;
    }

    return true;
}

bool adbc_send_json_raw(const int fd, const char *json)
{
    uint16_t length_be = 0U;
    size_t json_len = 0U;

    if(fd < 0 || json == NULL)
    {
        ADBC_ERROR("invalid JSON send arguments");
        return false;
    }

    while(json_len <= ADBC__MAX_JSON_SIZE && json[json_len] != '\0')
        json_len++;

    if(json_len == 0U || json_len > ADBC__MAX_JSON_SIZE)
    {
        ADBC_ERROR("invalid JSON payload size: %zu", json_len);
        return false;
    }

    length_be = htons((uint16_t)json_len);
    return adbc__send_all(fd, &length_be, sizeof(length_be)) &&
        adbc__send_all(fd, json, json_len);
}

bool adbc_send_json(const int fd, yyjson_mut_doc *doc)
{
    bool result = false;
    char *json = NULL;
    size_t json_len = 0U;

    if(fd < 0 || doc == NULL)
        return false;

    json = yyjson_mut_write(doc, 0, &json_len);
    if(json == NULL)
    {
        ADBC_ERROR("failed to serialize JSON node");
        return false;
    }

    if(json_len == 0U || json_len > ADBC__MAX_JSON_SIZE)
    {
        ADBC_ERROR("serialized JSON payload size is invalid: %zu", json_len);
        goto cleanup;
    }

    result = adbc_send_json_raw(fd, json);

cleanup:
    free(json);
    return result;
}
