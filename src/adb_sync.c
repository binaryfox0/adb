#include <adb/adb_sync.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "adb_decomp.h"
#include "adb_comp.h"
#include "adb_sync_proto.h"
#include "adb_log_priv.h"
#include "adb_utils.h"
#include "adb_stream_priv.h"
#include "adb_alloc_priv.h"

#define ADB__SYNC_ID_LSTAT_V1 ADB__CMD_ENCODE('S', 'T', 'A', 'T')
#define ADB__SYNC_ID_STAT_V2  ADB__CMD_ENCODE('S', 'T', 'A', '2')
#define ADB__SYNC_ID_LSTAT_V2 ADB__CMD_ENCODE('L', 'S', 'T', '2')
#define ADB__SYNC_ID_LIST_V1  ADB__CMD_ENCODE('L', 'I', 'S', 'T')
#define ADB__SYNC_ID_LIST_V2  ADB__CMD_ENCODE('L', 'I', 'S', '2')
#define ADB__SYNC_ID_DENT_V1  ADB__CMD_ENCODE('D', 'E', 'N', 'T')
#define ADB__SYNC_ID_DENT_V2  ADB__CMD_ENCODE('D', 'N', 'T', '2')

#define ADB__SYNC_ID_RECV_V1 ADB__CMD_ENCODE('R', 'E', 'C', 'V')
#define ADB__SYNC_ID_RECV_V2 ADB__CMD_ENCODE('R', 'C', 'V', '2')
#define ADB__SYNC_ID_SEND_V1 ADB__CMD_ENCODE('S', 'E', 'N', 'D')
#define ADB__SYNC_ID_SEND_V2 ADB__CMD_ENCODE('S', 'N', 'D', '2')

#define ADB__SYNC_ID_DONE ADB__CMD_ENCODE('D', 'O', 'N', 'E')
#define ADB__SYNC_ID_DATA ADB__CMD_ENCODE('D', 'A', 'T', 'A')
#define ADB__SYNC_ID_OKAY ADB__CMD_ENCODE('O', 'K', 'A', 'Y')
#define ADB__SYNC_ID_FAIL ADB__CMD_ENCODE('F', 'A', 'I', 'L')
#define ADB__SYNC_ID_QUIT ADB__CMD_ENCODE('Q', 'U', 'I', 'T')

#define ADB__SYNC_DATA_MAX (64 * 1024)
#define ADB__PUSH_IN_BUFFER_SIZE (256 * 1024)
#define ADB__PUSH_WAIT ((adb_error_t)ADB__ERR_COUNT)
typedef struct __attribute__((packed))
{
    uint32_t id;
    uint32_t path_len;
    /* followed by `path_length` bytes of non-null terminated path */
} adb__sync_request_t;

typedef struct __attribute__((packed))
{
    uint32_t id;
    uint32_t mode;
    uint32_t size;
    uint32_t mtime;
} adb__sync_stat_v1_t;

typedef struct __attribute__((packed))
{
    uint32_t id;
    uint32_t mode;
    uint32_t size;
    uint32_t mtime;
    uint32_t namelen;
} adb__sync_dent_v1_t;

typedef struct __attribute__((packed))
{
    uint32_t id;
    uint32_t error;
    uint64_t dev;
    uint64_t ino;
    uint32_t mode;
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    int64_t atime;
    int64_t mtime;
    int64_t ctime;
    uint32_t namelen;
    /* followed by `namelen` bytes of name */
} adb__sync_dent_v2_t;


typedef struct __attribute__((packed))
{
    uint32_t id;
    uint32_t mode;
    uint32_t flags;
} adb__sync_send_v2_t;

typedef struct __attribute__((packed))
{
    uint32_t id;
    uint32_t flags;
} adb__sync_recv_v2_t;

typedef struct __attribute__((packed))
{
    uint32_t id;
    uint32_t size;
} adb__sync_data_t;

typedef enum
{
    ADB__SYNC_FLAG_NONE = 0,
    ADB__SYNC_FLAG_BROTLI = (1 << 0),
    ADB__SYNC_FLAG_LZ4 = (1 << 1),
    ADB__SYNC_FLAG_ZSTD = (1 << 2)
} adb__sync_flag_t;

ADB__INLINE adb_error_t adb__sync_error_from_errno(void)
{
    switch(errno)
    {
        case ENOENT: return ADB_ERR_NOT_FOUND;
        case ENOMEM: return ADB_ERR_NO_MEM;
        case EINVAL: return ADB_ERR_PARAM;
        case ENAMETOOLONG:
        case EFBIG: return ADB_ERR_TOO_LONG;
        case ETIMEDOUT: return ADB_ERR_TIMEOUT;
        case ECONNRESET:
        case EPIPE:
        case ENOTCONN: return ADB_ERR_DISCONNECTED;
        default: return ADB_ERR_IO;
    }
}

static adb_error_t adb__send_request(
        adb__sync_t *sync,
        const uint32_t id,
        const char *path)
{
    size_t path_len = 0U;
    adb__sync_request_t req = {0};
    uint8_t payload[sizeof(req) + ADB_REMOTE_PATH_MAX] = {0};
    void *cursor = NULL;
    if(!sync || !path)
        return ADB_ERR_PARAM;
    path_len = strlen(path);
    if(path_len > ADB_REMOTE_PATH_MAX)
        return ADB_ERR_TOO_LONG;
    req.id = id;
    req.path_len = (uint32_t)path_len;
    cursor = adb__mempcpy(payload, &req, sizeof(req));
    memcpy(cursor, path, path_len);
    return adb__sync_proto_write(sync, payload, sizeof(req) + path_len);
}

static adb_error_t adb__send_recv_v2(
        adb__sync_t *sync,
        const char *path,
        const uint32_t flags)
{
    size_t path_len = 0U;
    adb__sync_request_t req = {0};
    adb__sync_recv_v2_t msg = {0};
    uint8_t payload[sizeof(req) + ADB_REMOTE_PATH_MAX + sizeof(msg)] = {0};
    uint8_t *cursor = NULL;
    if(!sync || !path)
        return ADB_ERR_PARAM;
    path_len = strlen(path);
    if(path_len > ADB_REMOTE_PATH_MAX)
        return ADB_ERR_TOO_LONG;
    req.id = ADB__SYNC_ID_RECV_V2;
    req.path_len = (uint32_t)path_len;
    msg.id = ADB__SYNC_ID_RECV_V2;
    msg.flags = flags;
    cursor = adb__mempcpy(payload, &req, sizeof(req));
    cursor = adb__mempcpy(cursor, path, path_len);
    memcpy(cursor, &msg, sizeof(msg));
    return adb__sync_proto_write(sync, payload,
            sizeof(req) + path_len + sizeof(msg));
}

static adb_error_t adb__send_quit(adb__sync_t *sync)
{
    return adb__send_request(sync, ADB__SYNC_ID_QUIT, "");
}

typedef struct
{
    adb__sync_t sync;
    adb_write_fn write_fn;
    void *userdata;
    adb__decomp_t *decomp;
    bool use_v2;
    uint32_t recv_flags;
    char remote_path[ADB_REMOTE_PATH_MAX + 1U];
    bool data_done;
    bool quit_sent;
    size_t data_remaining;
    size_t header_written;
    adb__sync_data_t header;
} adb__sync_pull_state_t;

static adb_error_t adb__sync_pull_stream_packet(adb_stream_t *stream,
                                                const adb__packet_t *pkt,
                                                uint8_t *payload);
static void adb__sync_pull_stream_destroy(adb_stream_t *stream);

typedef enum
{
    ADB__PUSH_SEND_REQUEST,
    ADB__PUSH_SEND_DATA,
    ADB__PUSH_SEND_DONE,
    ADB__PUSH_SEND_QUIT,
    ADB__PUSH_WAIT_RESPONSE,
    ADB__PUSH_WAIT_QUIT_ACK,
    ADB__PUSH_WAIT_CLOSE,
    ADB__PUSH_FINISHED
} adb__push_phase_t;

typedef struct
{
    adb__comp_t *comp;
    adb_read_fn read_fn;
    void *userdata;
    size_t source_remaining;
    size_t input_offset;
    size_t input_remaining;
    uint32_t mode;
    uint32_t modified_time;
    uint32_t send_flags;
    adb__push_phase_t phase;
    bool use_v2;
    size_t response_header_written;
    size_t response_remaining;
    bool response_failed;
    adb__sync_data_t response_header;
    uint8_t input[ADB__PUSH_IN_BUFFER_SIZE];
    char remote_path[ADB_REMOTE_PATH_MAX + 1U];
} adb__sync_push_state_t;

static adb_error_t adb__push_stream_write(adb_stream_t *stream,
                                          const void *payload,
                                          const size_t payload_size)
{
    adb__packet_t pkt = {0};

    if(stream->delayed_ack)
    {
        if(stream->our_asb < 0 || (size_t)stream->our_asb < payload_size)
            return ADB__PUSH_WAIT;
    }
    else if(!stream->send_ready)
        return ADB__PUSH_WAIT;

    pkt.command = ADB__CMD_WRTE;
    pkt.arg0 = stream->local_id;
    pkt.arg1 = stream->remote_id;
    pkt.payload_size = (uint32_t)payload_size;
    adb_error_t res = adb__packet_write(stream->conn, &pkt, payload);
    if(res != ADB_ERR_OK)
        return res;

    if(stream->delayed_ack)
        stream->our_asb -= (int32_t)payload_size;
    else
        stream->send_ready = false;
    return ADB_ERR_OK;
}

static adb_error_t adb__push_stream_send_request(adb_stream_t *stream,
                                                 adb__sync_push_state_t *state)
{
    adb__sync_request_t req = {0};
    adb__sync_send_v2_t msg = {0};
    uint8_t payload[sizeof(req) + ADB_REMOTE_PATH_MAX + sizeof(msg)] = {0};
    uint8_t *cursor = NULL;
    size_t path_size = strlen(state->remote_path);
    size_t payload_size = sizeof(req) + path_size + sizeof(msg);

    req.id = ADB__SYNC_ID_SEND_V2;
    req.path_len = (uint32_t)path_size;
    msg.id = ADB__SYNC_ID_SEND_V2;
    msg.mode = state->mode;
    msg.flags = state->send_flags;
    cursor = adb__mempcpy(payload, &req, sizeof(req));
    cursor = adb__mempcpy(cursor, state->remote_path, path_size);
    memcpy(cursor, &msg, sizeof(msg));

    return adb__push_stream_write(stream, payload, payload_size);
}

static adb_error_t adb__push_stream_send_request_v1(
        adb_stream_t *stream,
        adb__sync_push_state_t *state)
{
    adb__sync_request_t req = {0};
    char send_path[ADB_REMOTE_PATH_MAX + 16U] = {0};
    uint8_t payload[sizeof(req) + ADB_REMOTE_PATH_MAX] = {0};
    int path_size = 0;
    uint8_t *cursor = NULL;

    path_size = snprintf(send_path, sizeof(send_path), "%s,%u",
            state->remote_path, (unsigned int)state->mode);
    if(path_size < 0 || (size_t)path_size >= sizeof(send_path) ||
        (size_t)path_size > ADB_REMOTE_PATH_MAX)
        return ADB_ERR_TOO_LONG;

    req.id = ADB__SYNC_ID_SEND_V1;
    req.path_len = (uint32_t)path_size;
    cursor = adb__mempcpy(payload, &req, sizeof(req));
    memcpy(cursor, send_path, (size_t)path_size);
    return adb__push_stream_write(stream, payload,
        sizeof(req) + (size_t)path_size);
}

static adb_error_t adb__push_stream_send_data_v1(
        adb_stream_t *stream,
        adb__sync_push_state_t *state)
{
    adb__sync_data_t header = {0};
    uint8_t payload[sizeof(header) + ADB__SYNC_DATA_MAX] = {0};
    size_t data_size = ADB__SYNC_DATA_MAX;
    int read_size = 0;

    if(stream->delayed_ack)
    {
        if(stream->our_asb <= (int32_t)sizeof(header))
            return ADB__PUSH_WAIT;
        data_size = ADB__MIN(data_size,
                (size_t)(stream->our_asb - (int32_t)sizeof(header)));
    }

    if(state->source_remaining == 0U)
    {
        state->phase = ADB__PUSH_SEND_DONE;
        return ADB_ERR_OK;
    }

    data_size = ADB__MIN(data_size, state->source_remaining);
    read_size = state->read_fn(state->userdata,
            payload + sizeof(header), data_size);
    if(read_size < 0)
        return (adb_error_t)-read_size;
    if((size_t)read_size != data_size)
        return ADB_ERR_IO;

    header.id = ADB__SYNC_ID_DATA;
    header.size = (uint32_t)data_size;
    memcpy(payload, &header, sizeof(header));
    adb_error_t res = adb__push_stream_write(stream, payload,
            sizeof(header) + data_size);
    if(res == ADB_ERR_OK)
        state->source_remaining -= data_size;
    return res;
}

static adb_error_t adb__push_stream_read_response_v1(
        adb_stream_t *stream,
        adb__sync_push_state_t *state,
        const uint8_t *payload,
        const size_t payload_size)
{
    size_t offset = 0U;

    if(state->response_failed)
    {
        size_t consumed = ADB__MIN(payload_size, state->response_remaining);
        state->response_remaining -= consumed;
        if(state->response_remaining == 0U)
        {
            stream->error = ADB_ERR_IO;
            stream->done = true;
        }
        return ADB_ERR_OK;
    }

    while(offset < payload_size &&
          state->response_header_written < sizeof(state->response_header))
    {
        size_t available = ADB__MIN(payload_size - offset,
                sizeof(state->response_header) -
                state->response_header_written);
        memcpy((uint8_t *)&state->response_header +
                state->response_header_written, payload + offset, available);
        state->response_header_written += available;
        offset += available;
    }

    if(state->response_header_written != sizeof(state->response_header))
        return ADB_ERR_OK;

    if(state->response_header.id == ADB__SYNC_ID_FAIL)
    {
        if(state->response_header.size > ADB__SYNC_DATA_MAX)
            return ADB_ERR_PROTOCOL;
        state->response_failed = true;
        state->response_remaining = state->response_header.size;
        state->response_remaining -= ADB__MIN(
                payload_size - offset, state->response_remaining);
        if(state->response_remaining == 0U)
        {
            stream->error = ADB_ERR_IO;
            stream->done = true;
        }
        return ADB_ERR_OK;
    }

    if(state->response_header.id != ADB__SYNC_ID_OKAY ||
            state->response_header.size != 0U || offset != payload_size)
        return ADB_ERR_PROTOCOL;

    state->phase = ADB__PUSH_WAIT_CLOSE;
    stream->close_pending = true;
    return ADB_ERR_OK;
}

static adb_error_t adb__push_stream_send_data(adb_stream_t *stream,
                                              adb__sync_push_state_t *state)
{
    adb_error_t res = ADB_ERR_OK;
    uint8_t payload[sizeof(adb__sync_data_t) + ADB__SYNC_DATA_MAX] = {0};
    size_t output_size = 0U;
    size_t output_capacity = ADB__SYNC_DATA_MAX;

    if(stream->delayed_ack)
    {
        if(stream->our_asb <= (int32_t)sizeof(adb__sync_data_t))
            return ADB__PUSH_WAIT;
        output_capacity = ADB__MIN(
            output_capacity,
            (size_t)(stream->our_asb - (int32_t)sizeof(adb__sync_data_t)));
    }

    while(output_size == 0U &&
          (state->source_remaining != 0U || state->input_remaining != 0U ||
           !adb__comp_is_done(state->comp)))
    {
        if(state->input_remaining == 0U && state->source_remaining != 0U)
        {
            size_t read_size =
                ADB__MIN(state->source_remaining, sizeof(state->input));
            int read_result =
                state->read_fn(state->userdata, state->input, read_size);
            if(read_result < 0)
                return (adb_error_t)-read_result;
            if((size_t)read_result != read_size)
                return ADB_ERR_IO;
            state->source_remaining -= read_size;
            state->input_offset = 0U;
            state->input_remaining = read_size;
        }

        if(state->input_remaining != 0U)
        {
            size_t input_used = 0U;
            res = adb__comp_compress(
                state->comp, state->input + state->input_offset,
                state->input_remaining, payload + sizeof(adb__sync_data_t),
                output_capacity, &input_used, &output_size);
            if(res != ADB_ERR_OK)
                return res;
            state->input_offset += input_used;
            state->input_remaining -= input_used;
            if(input_used == 0U && output_size == 0U)
                return ADB_ERR_COMPRESS;
        }
        else if(!adb__comp_is_done(state->comp))
        {
            res = adb__comp_finish(state->comp,
                                   payload + sizeof(adb__sync_data_t),
                                   output_capacity, &output_size);
            if(res != ADB_ERR_OK)
                return res;
            if(output_size == 0U && !adb__comp_is_done(state->comp))
                return ADB_ERR_COMPRESS;
        }
    }

    if(output_size == 0U)
    {
        state->phase = ADB__PUSH_SEND_DONE;
        return ADB_ERR_OK;
    }

    ((adb__sync_data_t *)payload)->id = ADB__SYNC_ID_DATA;
    ((adb__sync_data_t *)payload)->size = (uint32_t)output_size;
    res = adb__push_stream_write(stream, payload,
                                 sizeof(adb__sync_data_t) + output_size);
    if(res == ADB_ERR_OK && state->source_remaining == 0U &&
       state->input_remaining == 0U && adb__comp_is_done(state->comp))
        state->phase = ADB__PUSH_SEND_DONE;
    return res;
}

static adb_error_t adb__sync_push_stream_pump(adb_stream_t *stream,
                                              adb__sync_push_state_t *state)
{
    adb_error_t res = ADB_ERR_OK;
    adb__push_phase_t phase = ADB__PUSH_SEND_REQUEST;

    for(;;)
    {
        phase = state->phase;
        switch(phase)
        {
        case ADB__PUSH_SEND_REQUEST:
            res = state->use_v2 ?
                adb__push_stream_send_request(stream, state) :
                adb__push_stream_send_request_v1(stream, state);
            if(res == ADB_ERR_OK)
                state->phase = ADB__PUSH_SEND_DATA;
            break;
        case ADB__PUSH_SEND_DATA:
            res = state->use_v2 ?
                adb__push_stream_send_data(stream, state) :
                adb__push_stream_send_data_v1(stream, state);
            if(res == ADB_ERR_OK && state->phase == ADB__PUSH_SEND_DONE)
            {
                adb__sync_data_t done = {0};
                done.id = ADB__SYNC_ID_DONE;
                done.size = state->modified_time;
                res = adb__push_stream_write(stream, &done, sizeof(done));
                if(res == ADB_ERR_OK)
                    state->phase = state->use_v2 ?
                        ADB__PUSH_SEND_QUIT : ADB__PUSH_WAIT_RESPONSE;
            }
            break;
        case ADB__PUSH_SEND_DONE:
        {
            adb__sync_data_t done = {0};
            done.id = ADB__SYNC_ID_DONE;
            done.size = state->modified_time;
            res = adb__push_stream_write(stream, &done, sizeof(done));
            if(res == ADB_ERR_OK)
                state->phase = state->use_v2 ?
                    ADB__PUSH_SEND_QUIT : ADB__PUSH_WAIT_RESPONSE;
            break;
        }
        case ADB__PUSH_SEND_QUIT:
        {
            adb__sync_request_t quit = {0};
            quit.id = ADB__SYNC_ID_QUIT;
            res = adb__push_stream_write(stream, &quit, sizeof(quit));
            if(res == ADB_ERR_OK)
                state->phase = ADB__PUSH_WAIT_QUIT_ACK;
            break;
        }
        case ADB__PUSH_WAIT_QUIT_ACK:
        case ADB__PUSH_WAIT_CLOSE:
            return ADB_ERR_OK;
        case ADB__PUSH_FINISHED:
            return ADB_ERR_OK;
        case ADB__PUSH_WAIT_RESPONSE:
            return ADB_ERR_OK;
        default:
            return ADB_ERR_PROTOCOL;
        }

        if(res == ADB__PUSH_WAIT)
            return ADB_ERR_OK;
        if(res != ADB_ERR_OK)
            return res;
        if(phase == ADB__PUSH_SEND_REQUEST ||
                (!stream->delayed_ack &&
                 phase == ADB__PUSH_SEND_DATA &&
                 state->phase == ADB__PUSH_SEND_DATA))
            return ADB_ERR_OK;
    }
}

static adb_error_t adb__sync_push_stream_packet(adb_stream_t *stream,
                                                const adb__packet_t *pkt,
                                                uint8_t *payload)
{
    adb__sync_push_state_t *state = stream->opaque;

    if(pkt->arg0 != stream->remote_id || pkt->arg1 != stream->local_id)
        return ADB_ERR_PROTOCOL;
    if(pkt->command == ADB__CMD_CLSE)
    {
        if((state->phase == ADB__PUSH_WAIT_QUIT_ACK ||
                    state->phase == ADB__PUSH_WAIT_CLOSE) &&
                adb__packet_check_size(pkt, 0U))
        {
            if(!stream->close_sent)
                stream->close_pending = true;
            stream->done = true;
            return ADB_ERR_OK;
        }
        return ADB_ERR_DISCONNECTED;
    }
    if(pkt->command == ADB__CMD_WRTE)
    {
        if(!state->use_v2)
            return adb__push_stream_read_response_v1(stream, state,
                    payload, pkt->payload_size);
        if(pkt->payload_size >= sizeof(uint32_t))
        {
            uint32_t id = 0U;
            memcpy(&id, payload, sizeof(id));
            if(id == ADB__SYNC_ID_FAIL)
                return ADB_ERR_IO;
        }
        return ADB_ERR_OK;
    }
    if(pkt->command != ADB__CMD_OKAY)
        return ADB_ERR_PROTOCOL;

    if(state->phase == ADB__PUSH_WAIT_QUIT_ACK)
    {
        state->phase = ADB__PUSH_WAIT_CLOSE;
        stream->close_pending = true;
        return ADB_ERR_OK;
    }

    return adb__sync_push_stream_pump(stream, state);
}

static void adb__sync_push_stream_destroy(adb_stream_t *stream)
{
    adb__sync_push_state_t *state = stream->opaque;
    if(state)
    {
        if(state->comp)
            adb__comp_destroy(state->comp);
        adb__free(state);
    }
    adb__free(stream);
}

adb_error_t adb_sync_push_start(adb_stream_t **out, adb_conn_t *conn,
                                const char *remote_path, const uint32_t mode,
                                const uint32_t modified_time, const size_t size,
                                const adb_read_fn read_fn, void *userdata)
{
    static const struct
    {
        const char *feature;
        adb__comp_type_t comp_type;
        uint32_t send_flags;
    } orders[] = {
        {ADB__FEATURE_SENDRECV_V2_ZSTD, ADB__COMP_ZSTD, ADB__SYNC_FLAG_ZSTD},
        {ADB__FEATURE_SENDRECV_V2_LZ4, ADB__COMP_LZ4, ADB__SYNC_FLAG_LZ4},
        {ADB__FEATURE_SENDRECV_V2_BROTLI, ADB__COMP_BROTLI,
         ADB__SYNC_FLAG_BROTLI},
        {ADB__FEATURE_SENDRECV_V2, ADB__COMP_NONE, ADB__SYNC_FLAG_NONE}};
    adb_error_t res = ADB_ERR_OK;
    adb_stream_t *stream = NULL;
    adb__sync_push_state_t *state = NULL;
    size_t path_size = 0U;

    if(!out || !conn || !remote_path || !read_fn)
        return ADB_ERR_PARAM;
    *out = NULL;
    path_size = strlen(remote_path);
    if(path_size > ADB_REMOTE_PATH_MAX)
        return ADB_ERR_TOO_LONG;

    stream = adb__calloc(1U, sizeof(*stream));
    state = adb__calloc(1U, sizeof(*state));
    if(!stream || !state)
    {
        adb__free(stream);
        adb__free(state);
        return ADB_ERR_NO_MEM;
    }

    for(size_t i = 0U; i < ADB__ARRSZ(orders); i++)
    {
        if(!adb__conn_support(conn, orders[i].feature))
            continue;
        res = adb__comp_create(&state->comp, orders[i].comp_type);
        if(res == ADB_ERR_OK)
        {
            state->send_flags = orders[i].send_flags;
            state->use_v2 = true;
        }
        break;
    }
    if(res != ADB_ERR_OK)
        goto fail;

    stream->type = ADB__STREAM_SYNC_PUSH;
    stream->packet_fn = adb__sync_push_stream_packet;
    stream->destroy_fn = adb__sync_push_stream_destroy;
    stream->opaque = state;
    state->read_fn = read_fn;
    state->userdata = userdata;
    state->source_remaining = size;
    state->mode = mode;
    state->modified_time = modified_time;
    state->phase = ADB__PUSH_SEND_REQUEST;
    memcpy(state->remote_path, remote_path, path_size + 1U);

    res = adb__stream_open(stream, conn, "sync:", sizeof("sync:"));
    if(res != ADB_ERR_OK)
        goto fail;
    *out = stream;
    return ADB_ERR_OK;

fail:
    adb__sync_push_stream_destroy(stream);
    return res;
}

adb_error_t adb_sync_push_finalize(adb_stream_t *stream)
{
    adb_error_t res = ADB_ERR_OK;
    if(!stream)
        return ADB_ERR_PARAM;
    res = stream->error;
    adb_stream_close(stream);
    return res;
}

adb_error_t adb_sync_pull_start(adb_stream_t **out, adb_conn_t *conn,
                                const char *remote_path,
                                const adb_write_fn write_fn, void *userdata)
{
    static const struct
    {
        const char *feature;
        adb__decomp_type_t type;
    } orders[] = {{ADB__FEATURE_SENDRECV_V2_ZSTD, ADB__DECOMP_ZSTD},
                  {ADB__FEATURE_SENDRECV_V2_LZ4, ADB__DECOMP_LZ4},
                  {ADB__FEATURE_SENDRECV_V2_BROTLI, ADB__DECOMP_BROTLI},
                  {ADB__FEATURE_SENDRECV_V2, ADB__DECOMP_NONE}};
    static const uint32_t flags[ADB__DECOMP_COUNT] = {
        [ADB__DECOMP_NONE] = ADB__SYNC_FLAG_NONE,
        [ADB__DECOMP_ZSTD] = ADB__SYNC_FLAG_ZSTD,
        [ADB__DECOMP_LZ4] = ADB__SYNC_FLAG_LZ4,
        [ADB__DECOMP_BROTLI] = ADB__SYNC_FLAG_BROTLI};
    adb_error_t res = ADB_ERR_OK;
    adb_stream_t *stream = NULL;
    adb__sync_pull_state_t *state = NULL;
    size_t path_len = 0U;

    if(!out || !conn || !remote_path || !write_fn)
        return ADB_ERR_PARAM;
    *out = NULL;
    path_len = strlen(remote_path);
    if(path_len > ADB_REMOTE_PATH_MAX)
        return ADB_ERR_TOO_LONG;

    stream = adb__calloc(1U, sizeof(*stream));
    state = adb__calloc(1U, sizeof(*state));
    if(!stream || !state)
    {
        adb__free(stream);
        adb__free(state);
        return ADB_ERR_NO_MEM;
    }

    stream->type = ADB__STREAM_SYNC_PULL;
    stream->packet_fn = adb__sync_pull_stream_packet;
    stream->destroy_fn = adb__sync_pull_stream_destroy;
    stream->opaque = state;
    state->write_fn = write_fn;
    state->userdata = userdata;
    memcpy(state->remote_path, remote_path, path_len + 1U);

    for(size_t i = 0U; i < ADB__ARRSZ(orders); i++)
    {
        if(adb__conn_support(conn, orders[i].feature))
        {
            state->use_v2 = true;
            state->recv_flags = flags[orders[i].type];
            res = adb__decomp_create(&state->decomp, orders[i].type, write_fn,
                                     userdata);
            break;
        }
    }
    if(res != ADB_ERR_OK)
        goto fail;

    res = adb__stream_open(stream, conn, "sync:", sizeof("sync:"));
    if(res != ADB_ERR_OK)
        goto fail;
    state->sync.conn = conn;
    state->sync.delayed_ack = stream->delayed_ack;
    state->sync.our_asb = stream->our_asb;
    state->sync.local_id = stream->local_id;
    *out = stream;
    return ADB_ERR_OK;

fail:
    adb__sync_proto_close(&state->sync);
    adb__decomp_destroy(state->decomp);
    adb__free(state);
    adb__free(stream);
    return res;
}

static adb_error_t adb__sync_pull_stream_packet(adb_stream_t *base,
                                                const adb__packet_t *pkt,
                                                uint8_t *payload)
{
    adb__sync_pull_state_t *state = base->opaque;
    adb__sync_t *sync = &state->sync;
    adb_error_t res = ADB_ERR_OK;
    size_t offset = 0U;
    bool saw_done = false;

    if(!sync->opened)
    {
        if(pkt->arg1 != sync->local_id)
            return ADB_ERR_PROTOCOL;
        if(pkt->command == ADB__CMD_CLSE)
            return ADB_ERR_UNSUPPORTED;
        if(pkt->command != ADB__CMD_OKAY)
            return ADB_ERR_PROTOCOL;
        sync->remote_id = base->remote_id;
        res = adb__sync_proto_dispatch(sync, pkt);
        if(res != ADB_ERR_OK)
            return res;
        if(sync->delayed_ack)
        {
            if(!adb__packet_check_size(pkt, sizeof(sync->their_asb)))
                return ADB_ERR_PROTOCOL;
        }
        else if(!adb__packet_check_size(pkt, 0U))
            return ADB_ERR_PROTOCOL;
        sync->opened = true;
        sync->send_ready = true;
        if(state->use_v2)
            return adb__send_recv_v2(sync, state->remote_path,
                                     state->recv_flags);
        return adb__send_request(sync, ADB__SYNC_ID_RECV_V1,
                                 state->remote_path);
    }

    if(pkt->arg0 != sync->remote_id || pkt->arg1 != sync->local_id)
        return ADB_ERR_PROTOCOL;
    if(pkt->command == ADB__CMD_CLSE)
    {
        if(state->quit_sent && adb__packet_check_size(pkt, 0U))
        {
            if(!base->close_sent)
                base->close_pending = true;
            base->done = true;
            return ADB_ERR_OK;
        }
        return ADB_ERR_DISCONNECTED;
    }
    res = adb__sync_proto_dispatch(sync, pkt);
    if(res != ADB_ERR_OK)
        return res;
    if(pkt->command == ADB__CMD_OKAY)
    {
        sync->pkt_payload = payload;
        res = adb__sync_proto_handle_okay(sync);
        sync->pkt_payload = NULL;
        if(res == ADB_ERR_OK && state->quit_sent)
            base->close_pending = true;
        return res;
    }
    if(pkt->command != ADB__CMD_WRTE)
        return ADB_ERR_PROTOCOL;

    while(offset < pkt->payload_size && !state->data_done)
    {
        size_t remaining = pkt->payload_size - offset;
        size_t available = 0U;
        if(state->data_remaining != 0U)
        {
            available = ADB__MIN(remaining, state->data_remaining);
            if(state->use_v2)
                res = adb__decomp_decompress(state->decomp, payload + offset,
                                             available);
            else
            {
                int written = state->write_fn(state->userdata, payload + offset,
                                              available);
                res = written < 0 ? (adb_error_t)-written
                                  : ((size_t)written == available ? ADB_ERR_OK
                                                                  : ADB_ERR_IO);
            }
            if(res != ADB_ERR_OK)
                return res;
            offset += available;
            state->data_remaining -= available;
            continue;
        }

        available =
            ADB__MIN(remaining, sizeof(state->header) - state->header_written);
        memcpy((uint8_t *)&state->header + state->header_written,
               payload + offset, available);
        offset += available;
        state->header_written += available;
        if(state->header_written != sizeof(state->header))
            continue;
        state->header_written = 0U;
        if(state->header.id == ADB__SYNC_ID_DONE)
        {
            if(state->header.size != 0U || !sync->send_ready)
                return ADB_ERR_PROTOCOL;
            state->data_done = true;
            saw_done = true;
            continue;
        }
        if(state->header.id != ADB__SYNC_ID_DATA ||
           state->header.size > ADB__SYNC_DATA_MAX)
            return ADB_ERR_PROTOCOL;
        state->data_remaining = state->header.size;
    }

    if(saw_done)
    {
        res = adb__send_quit(sync);
        if(res != ADB_ERR_OK)
            return res;
        state->quit_sent = true;
    }
    return ADB_ERR_OK;
}

static void adb__sync_pull_stream_destroy(adb_stream_t *base)
{
    adb__sync_pull_state_t *state = base->opaque;
    if(state)
    {
        state->sync.opened = false;
        adb__sync_proto_close(&state->sync);
        adb__decomp_destroy(state->decomp);
        adb__free(state);
    }
    adb__free(base);
}


adb_error_t adb_sync_pull(adb_conn_t *conn, const char *remote_path,
                          const adb_write_fn write_fn, void *userdata)
{
    adb_error_t res = ADB_ERR_OK;
    adb_stream_t *stream = NULL;
    if(!conn || !remote_path || !write_fn)
        return ADB_ERR_PARAM;

    res = adb_sync_pull_start(&stream, conn, remote_path, write_fn, userdata);
    if(res != ADB_ERR_OK)
        return res;

    while(!adb_stream_done(stream))
    {
        res = adb_conn_stream_dispatch(conn);
        if(res == ADB_ERR_NOT_FOUND)
            continue;
        if(res != ADB_ERR_OK && !adb_stream_done(stream))
            break;
    }

    adb_stream_close(stream);
    return ADB_ERR_OK;
}

static int adb__pull_file_write(void *userdata, const uint8_t *data,
                                const size_t size)
{
    /* if byte count mismatched, default err will be IO */
    return (int)fwrite(data, 1, size, userdata);
}

adb_error_t adb_sync_pull_file(adb_conn_t *conn, const char *remote_path,
                               const char *local_path)
{
    adb_error_t res = ADB_ERR_OK;
    FILE *file = NULL;
    if(!conn || !remote_path || !local_path)
        return ADB_ERR_PARAM;

    file = fopen(local_path, "wb");
    if(!file)
        return adb__sync_error_from_errno();

    res = adb_sync_pull(conn, remote_path, adb__pull_file_write, file);
    if(res != ADB_ERR_OK)
    {
        fclose(file);
        goto fail;
    }

    if(fclose(file) != 0)
    {
        res = ADB_ERR_IO;
        goto fail;
    }
    return ADB_ERR_OK;

fail:
    if(res != ADB_ERR_OK)
        remove(local_path);
    return res;
}

adb_error_t adb_sync_push(
        adb_conn_t *conn,
        const char *remote_path,
        const uint32_t mode,
        const uint32_t modified_time,
        const size_t size,
        const adb_read_fn read_fn,
        void *userdata)
{
    adb_error_t res = ADB_ERR_OK;
    adb_stream_t *stream = NULL;

    res = adb_sync_push_start(&stream, conn, remote_path, mode,
            modified_time, size, read_fn, userdata);
    if(res != ADB_ERR_OK)
        return res;

    while(!adb_stream_done(stream))
    {
        res = adb_conn_stream_dispatch(conn);
        if(res == ADB_ERR_NOT_FOUND)
            continue;
        if(res != ADB_ERR_OK && !adb_stream_done(stream))
            break;
    }

    if(res != ADB_ERR_OK && !adb_stream_done(stream))
        stream->error = res;
    return adb_sync_push_finalize(stream);
}

static int adb__push_file_read(
        void *userdata,
        uint8_t *out,
        const size_t size)
{
    /* if byte count mismatched, default err will be IO */
    return (int)fread(out, 1, size, userdata);
}

adb_error_t adb_sync_push_file(
        adb_conn_t *conn,
        const char *local_path,
        const char *remote_path)
{
    adb_error_t res = ADB_ERR_OK;
    struct stat st = {0};
    FILE *file = NULL;
    if(!conn || !local_path || !remote_path)
        return ADB_ERR_PARAM;

    if(stat(local_path, &st) == -1)
    {
        adb__log_err_errno("failed to get file information");
        return adb__sync_error_from_errno();
    }

    file = fopen(local_path, "rb");
    if(!file)
        return adb__sync_error_from_errno();

    res = adb_sync_push(conn, remote_path,
            st.st_mode, (uint32_t)st.st_mtim.tv_sec,
            (size_t)st.st_size,
            adb__push_file_read, file);
    fclose(file);
    return res;
}

ADB__INLINE int adb__sync_error_to_errno(
        const uint32_t error)
{
    switch(error)
    {
        case 13: return EACCES;
        case 17: return EEXIST;
        case 14: return EFAULT;
        case 27: return EFBIG;
        case 4: return EINTR;
        case 22: return EINVAL;
        case 5: return EIO;
        case 21: return EISDIR;
        case 40: return ELOOP;
        case 24: return EMFILE;
        case 36: return ENAMETOOLONG;
        case 23: return ENFILE;
        case 2: return ENOENT;
        case 12: return ENOMEM;
        case 28: return ENOSPC;
        case 20: return ENOTDIR;
        case 75: return EOVERFLOW;
        case 1: return EPERM;
        case 30: return EROFS;
        case 26: return ETXTBSY;
        default: return EIO;
    }
}

static adb_error_t adb__sync_read_fixed_response(
        adb__sync_t *sync,
        void *out,
        const size_t out_size)
{
    adb_error_t res = ADB_ERR_OK;
    size_t written = 0U;

    if(!sync || !out)
        return ADB_ERR_PARAM;

    while(written < out_size)
    {
        uint8_t *pkt_payload = NULL;
        size_t payload_size = 0U;
        size_t pkt_offset = 0U;

        res = adb__sync_proto_read(sync);
        if(res != ADB_ERR_OK)
            return res;

        if(sync->pkt.command == ADB__CMD_OKAY)
        {
            res = adb__sync_proto_handle_okay(sync);
            if(res != ADB_ERR_OK)
                return res;
            continue;
        }

        if(sync->pkt.command != ADB__CMD_WRTE)
            return ADB_ERR_PROTOCOL;

        pkt_payload = sync->pkt_payload;
        payload_size = sync->pkt.payload_size;

        while(pkt_offset < payload_size && written < out_size)
        {
            size_t pkt_remaining = 0U;
            size_t out_remaining = 0U;
            size_t available = 0U;

            pkt_remaining = payload_size - pkt_offset;
            out_remaining = out_size - written;
            available = ADB__MIN(pkt_remaining, out_remaining);

            memcpy((uint8_t *)out + written,
                    pkt_payload + pkt_offset, available);

            pkt_offset += available;
            written += available;
        }

        res = adb__sync_proto_ack(sync);
        if(res != ADB_ERR_OK)
            return res;
    }

    return ADB_ERR_OK;
}

static adb_error_t adb_sync_stat_v1(
        adb_conn_t *conn,
        const char *remote_path,
        adb_sync_stat_t *out,
        int *out_errno)
{
    adb_error_t res = ADB_ERR_OK;
    adb__sync_t sync = {0};
    adb__sync_stat_v1_t tmp = {0};

    if(!conn || !remote_path || !out)
        return ADB_ERR_PARAM;

    (void)out_errno;

    ADB__INFO("getting file information from device at \"%s\"", remote_path);
    res = adb__sync_proto_open(&sync, conn);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__send_request(&sync,
            ADB__SYNC_ID_LSTAT_V1, remote_path);
    if(res != ADB_ERR_OK)
        goto cleanup;

    res = adb__sync_read_fixed_response(&sync,
            &tmp, sizeof(tmp));
    if(res != ADB_ERR_OK)
        goto cleanup;

    if(tmp.id != ADB__SYNC_ID_LSTAT_V1)
    {
        ADB__ERROR("unexpected response message ID");
        ADB__INFO("expected: 0x%08X (%.4s), got: 0x%08X (%.4s)",
                (uint32_t)ADB__SYNC_ID_LSTAT_V1,
                (uint8_t *)ADB__PTR(uint32_t, ADB__SYNC_ID_LSTAT_V1),
                tmp.id, (uint8_t *)&tmp.id);
        res = ADB_ERR_PROTOCOL;
        goto cleanup;
    }

    if(tmp.mode == 0U && tmp.size == 0U && tmp.mtime == 0U)
    {
        ADB__ERROR("failed to get file information from device");
        res = ADB_ERR_IO;
        goto cleanup;
    }

    out->mode = tmp.mode;
    out->size = tmp.size;
    out->ctime = tmp.mtime;
    out->mtime = tmp.mtime;

    (void)adb__send_quit(&sync);
    ADB__INFO("got file information successfully");

cleanup:
    adb__sync_proto_close(&sync);
    return res;
}
static adb_error_t adb_sync_stat_v2(
        adb_conn_t *conn,
        const char *remote_path,
        adb_sync_stat_t *out,
        int *out_errno)
{
    adb_error_t res = ADB_ERR_OK;
    adb__sync_t sync = {0};
    adb_sync_stat_t tmp = {0};
    int error_num = 0;

    if(!conn || !remote_path || !out)
        return ADB_ERR_PARAM;

    ADB__INFO("getting file information from device at \"%s\"", remote_path);
    res = adb__sync_proto_open(&sync, conn);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__send_request(&sync,
            ADB__SYNC_ID_STAT_V2, remote_path);
    if(res != ADB_ERR_OK)
        goto cleanup;

    res = adb__sync_read_fixed_response(&sync,
            &tmp, sizeof(tmp));
    if(res != ADB_ERR_OK)
        goto cleanup;

    if(tmp.id != ADB__SYNC_ID_STAT_V2 &&
            tmp.id != ADB__SYNC_ID_LSTAT_V2)
    {
        ADB__ERROR("unexpected response message ID");
        ADB__INFO("expected: 0x%08X/0x%08X (%.4s/%.4s), got: 0x%08X (%.4s)",
                (uint32_t)ADB__SYNC_ID_STAT_V2,
                (uint32_t)ADB__SYNC_ID_LSTAT_V2,
                (uint8_t *)ADB__PTR(uint32_t, ADB__SYNC_ID_STAT_V2),
                (uint8_t *)ADB__PTR(uint32_t, ADB__SYNC_ID_LSTAT_V2),
                tmp.id, (uint8_t *)&tmp.id);
        res = ADB_ERR_PROTOCOL;
        goto cleanup;
    }

    if(tmp.error != 0U)
    {
        ADB__ERROR("failed to get file information from device");

        error_num = adb__sync_error_to_errno(tmp.error);
        if(out_errno)
            *out_errno = error_num;

        ADB__INFO("reason: %s", strerror(error_num));
        res = ADB_ERR_IO;
        goto cleanup;
    }

    *out = tmp;
    (void)adb__send_quit(&sync);
    ADB__INFO("got file information successfully");

cleanup:
    adb__sync_proto_close(&sync);
    return res;
}
adb_error_t adb_sync_stat(
        adb_conn_t *conn,
        const char *remote_path,
        adb_sync_stat_t *out,
        int *out_errno)
{
    if(!conn || !remote_path || !out)
        return ADB_ERR_PARAM;

    if(out_errno)
        *out_errno = 0;
    if(adb__conn_support(conn, ADB__FEATURE_STAT_V2))
        return adb_sync_stat_v2(conn, remote_path, out, out_errno);
    else
        return adb_sync_stat_v1(conn, remote_path, out, out_errno);
}

typedef enum
{
    ADB__LIST_RECORD_ID,
    ADB__LIST_RECORD_HEADER,
    ADB__LIST_RECORD_NAME,
    ADB__LIST_RECORD_DONE
} adb__sync_list_stage_t;

typedef enum
{
    ADB__LIST_WAIT_OPEN,
    ADB__LIST_WAIT_REQUEST_ACK,
    ADB__LIST_RECEIVING,
    ADB__LIST_WAIT_QUIT_ACK,
    ADB__LIST_WAIT_CLOSE
} adb__sync_list_phase_t;

typedef struct
{
    adb_sync_list_fn callback;
    void *userdata;
    bool use_v2;
    adb__sync_list_stage_t stage;
    adb__sync_list_phase_t phase;
    size_t record_size;
    size_t record_written;
    size_t header_size;
    uint32_t name_length;
    uint8_t record[sizeof(adb__sync_dent_v2_t) + ADB_REMOTE_PATH_MAX];
    adb_sync_stat_t info;
    char remote_path[ADB_REMOTE_PATH_MAX + 1U];
} adb__sync_list_state_t;

static adb_error_t adb__sync_list_send_request(
        adb_stream_t *stream,
        adb__sync_list_state_t *state)
{
    adb__sync_request_t req = {0};
    uint8_t payload[sizeof(req) + ADB_REMOTE_PATH_MAX] = {0};
    size_t path_len = strlen(state->remote_path);

    req.id = state->use_v2 ? ADB__SYNC_ID_LIST_V2 : ADB__SYNC_ID_LIST_V1;
    req.path_len = (uint32_t)path_len;
    memcpy(payload, &req, sizeof(req));
    memcpy(payload + sizeof(req), state->remote_path, path_len);
    state->phase = ADB__LIST_WAIT_REQUEST_ACK;
    return adb__push_stream_write(stream, payload, sizeof(req) + path_len);
}

static adb_error_t adb__sync_list_send_quit(
        adb_stream_t *stream,
        adb__sync_list_state_t *state)
{
    adb__sync_request_t quit = {0};

    quit.id = ADB__SYNC_ID_QUIT;
    state->phase = ADB__LIST_WAIT_QUIT_ACK;
    return adb__push_stream_write(stream, &quit, sizeof(quit));
}

static adb_error_t adb__sync_list_process_data(
        adb_stream_t *stream,
        adb__sync_list_state_t *state,
        const uint8_t *payload,
        const size_t payload_size)
{
    size_t offset = 0U;

    while(offset < payload_size)
    {
        size_t target_size = state->record_size;
        size_t needed = target_size - state->record_written;
        size_t available = ADB__MIN(payload_size - offset, needed);

        memcpy(state->record + state->record_written, payload + offset,
                available);
        state->record_written += available;
        offset += available;
        if(state->record_written != target_size)
            continue;

        if(state->stage == ADB__LIST_RECORD_ID)
        {
            uint32_t id = 0U;
            memcpy(&id, state->record, sizeof(id));
            if(id == ADB__SYNC_ID_DONE)
            {
                state->stage = ADB__LIST_RECORD_DONE;
                state->record_size = state->use_v2 ?
                    sizeof(adb__sync_dent_v2_t) :
                    sizeof(adb__sync_dent_v1_t);
                continue;
            }
            if(id != (state->use_v2 ? ADB__SYNC_ID_DENT_V2 :
                        ADB__SYNC_ID_DENT_V1))
                return ADB_ERR_PROTOCOL;

            state->stage = ADB__LIST_RECORD_HEADER;
            state->header_size = state->record_size = state->use_v2 ?
                sizeof(adb__sync_dent_v2_t) :
                sizeof(adb__sync_dent_v1_t);
            continue;
        }

        if(state->stage == ADB__LIST_RECORD_HEADER)
        {
            if(state->use_v2)
            {
                adb__sync_dent_v2_t dent = {0};
                memcpy(&dent, state->record, sizeof(dent));
                state->info.error = dent.error;
                state->info.dev = dent.dev;
                state->info.ino = dent.ino;
                state->info.mode = dent.mode;
                state->info.nlink = dent.nlink;
                state->info.uid = dent.uid;
                state->info.gid = dent.gid;
                state->info.size = dent.size;
                state->info.atime = dent.atime;
                state->info.mtime = dent.mtime;
                state->info.ctime = dent.ctime;
                state->name_length = dent.namelen;
            }
            else
            {
                adb__sync_dent_v1_t dent = {0};
                memcpy(&dent, state->record, sizeof(dent));
                state->info.mode = dent.mode;
                state->info.size = dent.size;
                state->info.mtime = dent.mtime;
                state->info.ctime = dent.mtime;
                state->name_length = dent.namelen;
            }

            if(state->name_length == 0U ||
                    state->name_length > ADB_REMOTE_PATH_MAX)
                return ADB_ERR_PROTOCOL;
            state->stage = ADB__LIST_RECORD_NAME;
            state->record_size = state->header_size + state->name_length;
            continue;
        }

        if(state->stage == ADB__LIST_RECORD_NAME)
        {
            char name[ADB_REMOTE_PATH_MAX + 1U] = {0};
            int error_num = state->info.error != 0U ?
                adb__sync_error_to_errno(state->info.error) : 0;

            memcpy(name, state->record + state->header_size,
                    state->name_length);
                if(memchr(name, '\0', state->name_length) != NULL ||
                    memchr(name, '/', state->name_length) != NULL)
                return ADB_ERR_PROTOCOL;
            {
                adb_error_t res = state->callback(state->userdata, name,
                        &state->info, error_num);
                if(res != ADB_ERR_OK)
                    return res;
            }

            state->stage = ADB__LIST_RECORD_ID;
            state->record_size = sizeof(uint32_t);
            state->record_written = 0U;
            state->info = (adb_sync_stat_t){0};
            continue;
        }

        if(state->stage == ADB__LIST_RECORD_DONE)
        {
            for(size_t i = sizeof(uint32_t); i < state->record_size; i++)
            {
                if(state->record[i] != 0U)
                    return ADB_ERR_PROTOCOL;
            }
            if(offset != payload_size)
                return ADB_ERR_PROTOCOL;
            state->phase = ADB__LIST_WAIT_QUIT_ACK;
            return adb__sync_list_send_quit(stream, state);
        }
    }

    return ADB_ERR_OK;
}

static adb_error_t adb__sync_list_stream_packet(
        adb_stream_t *stream,
        const adb__packet_t *pkt,
        uint8_t *payload)
{
    adb__sync_list_state_t *state = stream->opaque;

    if(state->phase == ADB__LIST_WAIT_OPEN)
    {
        if(pkt->arg1 != stream->local_id)
            return ADB_ERR_PROTOCOL;
        if(pkt->command == ADB__CMD_CLSE)
            return ADB_ERR_UNSUPPORTED;
        if(pkt->command != ADB__CMD_OKAY ||
                pkt->arg0 != stream->remote_id)
            return ADB_ERR_PROTOCOL;
        return adb__sync_list_send_request(stream, state);
    }

    if(pkt->arg0 != stream->remote_id || pkt->arg1 != stream->local_id)
        return ADB_ERR_PROTOCOL;

    if(pkt->command == ADB__CMD_CLSE)
    {
        if((state->phase == ADB__LIST_WAIT_QUIT_ACK ||
                    state->phase == ADB__LIST_WAIT_CLOSE) &&
                adb__packet_check_size(pkt, 0U))
        {
            if(!stream->close_sent)
                stream->close_pending = true;
            stream->done = true;
            return ADB_ERR_OK;
        }
        return ADB_ERR_DISCONNECTED;
    }

    if(pkt->command == ADB__CMD_OKAY)
    {
        if(state->phase == ADB__LIST_WAIT_REQUEST_ACK)
        {
            state->phase = ADB__LIST_RECEIVING;
            return ADB_ERR_OK;
        }
        if(state->phase == ADB__LIST_WAIT_QUIT_ACK)
        {
            state->phase = ADB__LIST_WAIT_CLOSE;
            stream->close_pending = true;
            return ADB_ERR_OK;
        }
        return ADB_ERR_PROTOCOL;
    }

    if(pkt->command == ADB__CMD_WRTE &&
            state->phase == ADB__LIST_RECEIVING)
        return adb__sync_list_process_data(stream, state, payload,
                pkt->payload_size);

    return ADB_ERR_PROTOCOL;
}

static void adb__sync_list_stream_destroy(adb_stream_t *stream)
{
    adb__free(stream->opaque);
    adb__free(stream);
}

static adb_error_t adb__sync_list_stream(
        adb_conn_t *conn,
        const char *remote_path,
        const adb_sync_list_fn callback,
        void *userdata,
        const bool use_v2)
{
    adb_error_t res = ADB_ERR_OK;
    adb_stream_t *stream = adb__calloc(1U, sizeof(*stream));
    adb__sync_list_state_t *state = adb__calloc(1U, sizeof(*state));

    if(!stream || !state)
    {
        adb__free(stream);
        adb__free(state);
        return ADB_ERR_NO_MEM;
    }

    stream->type = ADB__STREAM_SYNC_LIST;
    stream->packet_fn = adb__sync_list_stream_packet;
    stream->destroy_fn = adb__sync_list_stream_destroy;
    stream->opaque = state;
    state->callback = callback;
    state->userdata = userdata;
    state->use_v2 = use_v2;
    state->stage = ADB__LIST_RECORD_ID;
    state->phase = ADB__LIST_WAIT_OPEN;
    state->record_size = sizeof(uint32_t);
    memcpy(state->remote_path, remote_path, strlen(remote_path) + 1U);

    res = adb__stream_open(stream, conn, "sync:", sizeof("sync:"));
    if(res != ADB_ERR_OK)
        goto cleanup;

    while(!stream->done)
    {
        res = adb_conn_stream_dispatch(conn);
        if(res == ADB_ERR_NOT_FOUND)
            continue;
        if(res != ADB_ERR_OK && !stream->done)
        {
            stream->error = res;
            stream->done = true;
        }
    }
    res = stream->error;

cleanup:
    adb_stream_close(stream);
    return res;
}

adb_error_t adb_sync_list(
        adb_conn_t *conn,
        const char *remote_path,
        const adb_sync_list_fn callback,
        void *userdata)
{
    if(!conn || !remote_path || !callback)
        return ADB_ERR_PARAM;

    if(strlen(remote_path) > ADB_REMOTE_PATH_MAX)
        return ADB_ERR_TOO_LONG;
    return adb__sync_list_stream(conn, remote_path, callback, userdata,
            adb__conn_support(conn, ADB__FEATURE_LS_V2));
}
