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

#define ADB__SYNC_ID_RECV_V1 ADB__CMD_ENCODE('R', 'E', 'C', 'V')
#define ADB__SYNC_ID_RECV_V2 ADB__CMD_ENCODE('R', 'C', 'V', '2')
#define ADB__SYNC_ID_SEND_V2 ADB__CMD_ENCODE('S', 'N', 'D', '2')

#define ADB__SYNC_ID_DONE ADB__CMD_ENCODE('D', 'O', 'N', 'E')
#define ADB__SYNC_ID_DATA ADB__CMD_ENCODE('D', 'A', 'T', 'A')
#define ADB__SYNC_ID_OKAY ADB__CMD_ENCODE('O', 'K', 'A', 'Y')
#define ADB__SYNC_ID_FAIL ADB__CMD_ENCODE('F', 'A', 'I', 'L')
#define ADB__SYNC_ID_QUIT ADB__CMD_ENCODE('Q', 'U', 'I', 'T')

#define ADB__SYNC_DATA_MAX (64 * 1024)
#define ADB__INIT_DELAYED_ACK_BYTES (32 * 1024 * 1024)

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
    /* followed by `size` bytes of data. */
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
        case ENOENT:    return ADB_ERR_NOT_FOUND;
        case ENOMEM:    return ADB_ERR_NO_MEM;
        case EINVAL:    return ADB_ERR_PARAM;
        case ENAMETOOLONG:
        case EFBIG:     return ADB_ERR_TOO_LONG;
        case ETIMEDOUT: return ADB_ERR_TIMEOUT;
        case ECONNRESET:
        case EPIPE:
        case ENOTCONN:  return ADB_ERR_DISCONNECTED;
        default:        return ADB_ERR_IO;
    }
}

static adb_error_t adb__send_request(
        adb__sync_t *sync,
        const uint32_t id,
        const char *path)
{
    size_t path_len = 0;
    adb__sync_request_t req = {0};
    uint8_t req_payload[sizeof(req) + ADB_REMOTE_PATH_MAX] = {0};
    void *req_cursor = NULL;
    if(!sync || !path)
        return ADB_ERR_PARAM;

    path_len = strlen(path);
    if(path_len > ADB_REMOTE_PATH_MAX)
        return ADB_ERR_TOO_LONG;

    req.id = id;
    req.path_len = (uint32_t)path_len;
    req_cursor = adb__mempcpy(req_payload, &req, sizeof(req));
    memcpy(req_cursor, path, path_len);

    return adb__sync_proto_write(sync, 
            req_payload, sizeof(req) + path_len);
}

static adb_error_t adb__send_recv_v2(
        adb__sync_t *sync,
        const char *path,
        const uint32_t flags)
{ 
    size_t path_len = 0;
    size_t req_size = 0;
    adb__sync_request_t req = {0};
    adb__sync_recv_v2_t msg = {0};

    uint8_t req_payload[
            sizeof(req) +
            ADB_REMOTE_PATH_MAX +
            sizeof(msg)] = {0};
    uint8_t *req_cursor = NULL;

    if(!sync || !path)
        return ADB_ERR_PARAM;

    path_len = strlen(path);
    if(path_len > ADB_REMOTE_PATH_MAX)
        return ADB_ERR_TOO_LONG;
    
    req_size = sizeof(req) + path_len + sizeof(msg);

    req.id = ADB__SYNC_ID_RECV_V2;
    req.path_len = (uint32_t)path_len;
    msg.id = ADB__SYNC_ID_RECV_V2;
    msg.flags = flags;

    req_cursor = adb__mempcpy(req_payload, &req, sizeof(req));
    req_cursor = adb__mempcpy(req_cursor, path, path_len);
    memcpy(req_cursor, &msg, sizeof(msg));
    
    return adb__sync_proto_write(sync, 
            req_payload, req_size);
}

static adb_error_t adb__send_quit(
        adb__sync_t *sync) {
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
    size_t data_remaining;
    size_t header_written;
    adb__sync_data_t header;
} adb__sync_pull_state_t;

static adb_error_t adb__sync_pull_stream_packet(
        adb_stream_t *stream,
        const adb__packet_t *pkt,
        uint8_t *payload);

static void adb__sync_pull_stream_destroy(
        adb_stream_t *stream);

adb_error_t adb_sync_pull_start(
        adb_stream_t **out,
        adb_conn_t *conn,
        const char *remote_path,
        const adb_write_fn write_fn,
        void *userdata)
{
    static const struct {
        const char *feature;
        adb__decomp_type_t type;
    } orders[] = {
        { ADB__FEATURE_SENDRECV_V2_ZSTD, ADB__DECOMP_ZSTD },
        { ADB__FEATURE_SENDRECV_V2_LZ4, ADB__DECOMP_LZ4 },
        { ADB__FEATURE_SENDRECV_V2_BROTLI, ADB__DECOMP_BROTLI },
        { ADB__FEATURE_SENDRECV_V2, ADB__DECOMP_NONE }
    };
    static const uint32_t flags[ADB__DECOMP_COUNT] = {
        [ADB__DECOMP_NONE] = 0,
        [ADB__DECOMP_ZSTD] = ADB__SYNC_FLAG_ZSTD,
        [ADB__DECOMP_LZ4] = ADB__SYNC_FLAG_LZ4,
        [ADB__DECOMP_BROTLI] = ADB__SYNC_FLAG_BROTLI
    };
    adb_error_t res = ADB_ERR_OK;
    adb_stream_t *stream = NULL;
    adb__sync_pull_state_t *state = NULL;
    size_t path_len = 0;

    if(!out || !conn || !remote_path || !write_fn)
        return ADB_ERR_PARAM;
    *out = NULL;
    path_len = strlen(remote_path);
    if(path_len > ADB_REMOTE_PATH_MAX)
        return ADB_ERR_TOO_LONG;

    stream = adb__calloc(1, sizeof(*stream));
    state = adb__calloc(1, sizeof(*state));
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

    for(size_t i = 0; i < ADB__ARRSZ(orders); i++)
    {
        if(adb__conn_support(conn, orders[i].feature))
        {
            state->use_v2 = true;
            state->recv_flags = flags[orders[i].type];
            res = adb__decomp_create(&state->decomp,
                    orders[i].type, write_fn, userdata);
            break;
        }
    }
    if(res != ADB_ERR_OK)
        goto fail;

    res = adb__sync_proto_open_start(&state->sync, conn);
    if(res != ADB_ERR_OK)
        goto fail;

    stream->local_id = state->sync.local_id;
    res = adb__conn_stream_add(conn, stream);
    if(res != ADB_ERR_OK)
        goto fail;

    *out = stream;
    return ADB_ERR_OK;

fail:
    adb__sync_proto_close(&state->sync);
    adb__decomp_destroy(state->decomp);
    adb__free(state);
    adb__free(stream);
    return res;
}

static adb_error_t adb__sync_pull_stream_packet(
        adb_stream_t *base,
        const adb__packet_t *pkt,
        uint8_t *payload)
{
    adb__sync_pull_state_t *state = base->opaque;
    adb__sync_t *sync = &state->sync;
    adb_error_t res = ADB_ERR_OK;
    size_t offset = 0;
    bool saw_done = false;

    if(!sync->opened)
    {
        if(pkt->arg1 != sync->local_id)
            return ADB_ERR_PROTOCOL;
        if(pkt->command == ADB__CMD_CLSE)
            return ADB_ERR_UNSUPPORTED;
        if(pkt->command != ADB__CMD_OKAY)
            return ADB_ERR_PROTOCOL;

        sync->remote_id = pkt->arg0;
        res = adb__sync_proto_dispatch(sync, pkt);
        if(res != ADB_ERR_OK)
            return res;

        if(sync->delayed_ack)
        {
            if(!adb__packet_check_size(pkt, sizeof(sync->their_asb)))
                return ADB_ERR_PROTOCOL;
            memcpy(&sync->their_asb, payload, sizeof(sync->their_asb));
        }
        else if(!adb__packet_check_size(pkt, 0))
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
        return ADB_ERR_DISCONNECTED;

    res = adb__sync_proto_dispatch(sync, pkt);
    if(res != ADB_ERR_OK)
        return res;
    if(pkt->command == ADB__CMD_OKAY)
    {
        sync->pkt_payload = payload;
        res = adb__sync_proto_handle_okay(sync);
        sync->pkt_payload = NULL;
        return res;
    }
    if(pkt->command != ADB__CMD_WRTE)
        return ADB_ERR_PROTOCOL;

    while(offset < pkt->payload_size && !state->data_done)
    {
        size_t remaining = pkt->payload_size - offset;
        size_t available = 0;
        if(state->data_remaining != 0)
        {
            available = ADB__MIN(remaining, state->data_remaining);
            if(state->use_v2)
                res = adb__decomp_decompress(state->decomp,
                        payload + offset, available);
            else
            {
                int written = state->write_fn(state->userdata,
                        payload + offset, available);
                res = written < 0 ? (adb_error_t)-written :
                    ((size_t)written == available ? ADB_ERR_OK : ADB_ERR_IO);
            }
            if(res != ADB_ERR_OK)
                return res;
            offset += available;
            state->data_remaining -= available;
            continue;
        }

        available = ADB__MIN(remaining,
                sizeof(state->header) - state->header_written);
        memcpy((uint8_t*)&state->header + state->header_written,
                payload + offset, available);
        offset += available;
        state->header_written += available;
        if(state->header_written != sizeof(state->header))
            continue;

        state->header_written = 0;
        if(state->header.id == ADB__SYNC_ID_DONE)
        {
            if(state->header.size != 0 || !sync->send_ready)
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

    res = adb__sync_proto_ack(sync);
    if(res != ADB_ERR_OK)
        return res;
    if(saw_done)
    {
        res = adb__send_quit(sync);
        if(res != ADB_ERR_OK)
            return res;
        base->done = true;
    }
    return ADB_ERR_OK;
}

static void adb__sync_pull_stream_destroy(
        adb_stream_t *base)
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

adb_error_t adb_sync_pull_finalize(
        adb_stream_t *stream)
{
    adb_error_t res = ADB_ERR_OK;
    if(!stream)
        return ADB_ERR_PARAM;
    res = stream->error;
    if(stream->conn)
        adb__conn_stream_remove(stream->conn, stream);
    if(stream->destroy_fn)
        stream->destroy_fn(stream);
    return res;
}

adb_error_t adb_sync_pull(
        adb_conn_t *conn,
        const char *remote_path,
        const adb_write_fn write_fn,
        void *userdata)
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

    return adb_sync_pull_finalize(stream);
}

static int adb__pull_file_write(
        void *userdata,
        const uint8_t *data,
        const size_t size)
{
    /* if byte count mismatched, default err will be IO */
    return (int)fwrite(data, 1, size, userdata);
}

adb_error_t adb_sync_pull_file(
        adb_conn_t *conn,
        const char *remote_path,
        const char *local_path)
{
    adb_error_t res = ADB_ERR_OK;
    FILE *file = NULL;
    if(!conn || !remote_path || !local_path)
        return ADB_ERR_PARAM;
     
    file = fopen(local_path, "wb");
    if(!file)
        return adb__sync_error_from_errno();

    res = adb_sync_pull(conn, remote_path, 
            adb__pull_file_write, file);
    if(res != ADB_ERR_OK)
        { fclose(file); goto fail; }

    if(fclose(file) != 0)
        { res = ADB_ERR_IO; goto fail; }
    return ADB_ERR_OK;

fail:
    if(res != ADB_ERR_OK)
        remove(local_path);
    return res;
}

static adb_error_t adb__send_send_v2(
        adb__sync_t *sync,
        const char *path,
        const uint32_t mode,
        const uint32_t flags)
{
    size_t path_len = 0;
    size_t req_size = 0;
    adb__sync_request_t req = {0};
    adb__sync_send_v2_t msg = {0};

    uint8_t req_payload[
            sizeof(req) +
            ADB_REMOTE_PATH_MAX +
            sizeof(msg)] = {0};
    uint8_t *req_cursor = NULL;

    if(!sync || !path)
        return ADB_ERR_PARAM;

    path_len = strlen(path);
    if(path_len > ADB_REMOTE_PATH_MAX)
        return ADB_ERR_TOO_LONG;
    
    req_size = sizeof(req) + path_len + sizeof(msg);

    req.id = ADB__SYNC_ID_SEND_V2;
    req.path_len = (uint32_t)path_len;
    msg.id = ADB__SYNC_ID_SEND_V2;
    msg.mode = mode;
    msg.flags = flags;

    req_cursor = adb__mempcpy(req_payload, &req, sizeof(req));
    req_cursor = adb__mempcpy(req_cursor, path, path_len);
    memcpy(req_cursor, &msg, sizeof(msg));
    
    return adb__sync_proto_write(sync, 
            req_payload, req_size);
}

#define ADB__PUSH_IN_BUFFER_SIZE (256 * 1024)

static adb_error_t adb__push_v1(
        adb_conn_t *conn,
        const char *remote_path,
        const uint32_t mode,
        const uint32_t modified_time,
        const size_t size,
        const adb_read_fn read_fn,
        void *userdata)
{
    (void)conn;
    (void)remote_path;
    (void)mode;
    (void)modified_time;
    (void)size;
    (void)read_fn;
    (void)userdata;

    return ADB_ERR_UNIMPLEMENTED;
}

static adb_error_t adb__push_v2(
        adb_conn_t *conn,
        const char *remote_path,
        const uint32_t mode,
        const uint32_t modified_time,
        const adb__comp_type_t comp_type,
        const size_t size,
        const adb_read_fn read_fn,
        void *userdata)
{
    static uint32_t comp_type_to_flags[ADB__COMP_COUNT] =
    {
        [ADB__COMP_NONE] = 0,
        [ADB__COMP_BROTLI] = ADB__SYNC_FLAG_BROTLI,
        [ADB__COMP_LZ4] = ADB__SYNC_FLAG_LZ4,
        [ADB__COMP_ZSTD] = ADB__SYNC_FLAG_ZSTD,
    };
    adb_error_t res = ADB_ERR_OK;
    adb__sync_t sync = {0};
    adb__comp_t *comp = NULL;
    size_t remaining = 0;
    uint8_t in_buf[ADB__PUSH_IN_BUFFER_SIZE] = {0};

    res = adb__sync_proto_open(&sync, conn);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__send_send_v2(&sync, remote_path, mode, comp_type_to_flags[comp_type]);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__comp_create(&comp, comp_type);
    if(res != ADB_ERR_OK)
        goto cleanup;

    remaining = size;
    while(remaining != 0)
    {
        size_t read_size = 0;
        int read_res = 0;
        size_t in_offset = 0;
        size_t in_remaining = 0;

        read_size = ADB__MIN(remaining, sizeof(in_buf));
        read_res = read_fn(userdata, in_buf, read_size);
        if(read_res < 0)
        {
            res = (adb_error_t)-read_res;
            goto cleanup;
        }

        if((size_t)read_res != read_size)
        {
            res = ADB_ERR_IO;
            goto cleanup;
        }

        in_remaining = (size_t)read_res;
        remaining -= in_remaining;
        while(in_remaining != 0)
        {
            uint8_t data_pkt[sizeof(adb__sync_data_t) + ADB__SYNC_DATA_MAX] = {0};
            size_t in_used = 0;
            size_t out_used = 0;
            
            res = adb__comp_compress(
                    comp,
                    in_buf + in_offset,
                    in_remaining,
                    data_pkt + sizeof(adb__sync_data_t),
                    sizeof(data_pkt) - sizeof(adb__sync_data_t),
                    &in_used,
                    &out_used);
            if(res != ADB_ERR_OK)
                goto cleanup;

            if(out_used != 0)
            {
                adb__sync_data_t *data = (adb__sync_data_t*)data_pkt;
                data->id = ADB__SYNC_ID_DATA;
                data->size = (uint32_t)out_used;

                res = adb__sync_proto_write(
                        &sync,
                        data_pkt,
                        sizeof(adb__sync_data_t) + out_used);
                if(res != ADB_ERR_OK)
                    goto cleanup;

                res = adb__sync_proto_check_ack(&sync);
                if(res != ADB_ERR_OK)
                    goto cleanup;
    
            }

            if(in_used == 0 && out_used == 0)
            {
                res = ADB_ERR_COMPRESS;
                goto cleanup;
            }

            in_offset += in_used;
            in_remaining -= in_used;
        }
    }
    
    while(!adb__comp_is_done(comp))
    {
        uint8_t data_pkt[sizeof(adb__sync_data_t) + ADB__SYNC_DATA_MAX] = {0};
        size_t out_used = 0;
        res = adb__comp_finish(
                comp,
                data_pkt + sizeof(adb__sync_data_t),
                sizeof(data_pkt) - sizeof(adb__sync_data_t),
                &out_used);
        if(res != ADB_ERR_OK)
            goto cleanup;

        if(out_used != 0)
        {
            adb__sync_data_t *data = (adb__sync_data_t*)data_pkt;
            data->id = ADB__SYNC_ID_DATA;
            data->size = (uint32_t)out_used;

            res = adb__sync_proto_write(
                    &sync,
                    data_pkt,
                    sizeof(adb__sync_data_t) + out_used);
            if(res != ADB_ERR_OK)
                goto cleanup;
               
            for(;;)
            {
                res = adb__sync_proto_read_timeout(&sync, 1);
                if(res == ADB_ERR_TIMEOUT)
                {
                    while(sync.our_asb < 0)
                    {
                        res = adb__sync_proto_read(&sync);
                        if(res != ADB_ERR_OK)
                            goto cleanup;
                        if(!adb__packet_check_cmd(&sync.pkt, ADB__CMD_OKAY))
                            return ADB_ERR_PROTOCOL;
                        res = adb__sync_proto_handle_okay(&sync);
                        if(res != ADB_ERR_OK)
                            return res;
                    }
                    break;
                }
                else if(res != ADB_ERR_OK)
                    goto cleanup;

                if(!adb__packet_check_cmd(&sync.pkt, ADB__CMD_OKAY))
                    return ADB_ERR_PROTOCOL;
                res = adb__sync_proto_handle_okay(&sync);
                if(res != ADB_ERR_OK)
                    return res;

            }
        }
    }

    {
        adb__sync_data_t done = {0};
        done.id = ADB__SYNC_ID_DONE;
        done.size = modified_time;
        res = adb__sync_proto_write(
                &sync,
                &done,
                sizeof(done));
        if(res != ADB_ERR_OK)
            goto cleanup;
    }

    (void)adb__send_quit(&sync);

cleanup:
    adb__sync_proto_close(&sync);
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
    struct {
        const char *feature;
        adb__comp_type_t comp_type;
    } check_orders[ADB__COMP_COUNT] =
    {
        { ADB__FEATURE_SENDRECV_V2_ZSTD, ADB__COMP_ZSTD },
        { ADB__FEATURE_SENDRECV_V2_LZ4, ADB__COMP_LZ4 },
        { ADB__FEATURE_SENDRECV_V2_BROTLI, ADB__COMP_BROTLI },
        { ADB__FEATURE_SENDRECV_V2, ADB__COMP_NONE },
    };

    if(!conn || !remote_path || !read_fn)
        return ADB_ERR_PARAM;
    
    for(size_t i = 0; i < ADB__ARRSZ(check_orders); i++)
    {
        if(adb__conn_support(conn, check_orders[i].feature))
        {
            return adb__push_v2(conn, remote_path, 
                    mode, modified_time, check_orders[i].comp_type, 
                    size, read_fn, userdata);
        }
    }

    return adb__push_v1(conn, remote_path, 
            mode, modified_time, size, read_fn, userdata);
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
