#include <adb/adb_sync.h>

#include <stdio.h>
#include <sys/stat.h>

#include "adb_decomp.h"
#include "adb_comp.h"
#include "adb_sync_proto.h"
#include "adb_log_priv.h"
#include "adb_utils.h"

#define ADB__PATH_MAX                   1024

#define ADB__SYNC_ID_RECV_V1 ADB__CMD_ENCODE('R', 'E', 'C', 'V')
#define ADB__SYNC_ID_RECV_V2 ADB__CMD_ENCODE('R', 'C', 'V', '2')
#define ADB__SYNC_ID_SEND_V2 ADB__CMD_ENCODE('S', 'N', 'D', '2')
#define ADB__SYNC_ID_DONE ADB__CMD_ENCODE('D', 'O', 'N', 'E')
#define ADB__SYNC_ID_DATA ADB__CMD_ENCODE('D', 'A', 'T', 'A')
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

typedef enum 
{
    ADB__SYNC_FLAG_NONE = 0,
    ADB__SYNC_FLAG_BROTLI = (1 << 0),
    ADB__SYNC_FLAG_LZ4 = (1 << 1),
    ADB__SYNC_FLAG_ZSTD = (1 << 2)
} adb__sync_flag_t;

static adb_error_t adb__send_request(
        adb__sync_t *sync,
        const uint32_t id,
        const char *path)
{
    size_t path_len = 0;
    adb__sync_request_t req = {0};
    uint8_t req_payload[sizeof(req) + ADB__PATH_MAX] = {0};
    void *req_cursor = NULL;
    if(!sync || !path)
        return ADB_ERR_PARAM;

    path_len = strlen(path);
    if(path_len > ADB__PATH_MAX)
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
            ADB__PATH_MAX +
            sizeof(msg)] = {0};
    uint8_t *req_cursor = NULL;

    if(!sync || !path)
        return ADB_ERR_PARAM;

    path_len = strlen(path);
    if(path_len > ADB__PATH_MAX)
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

static void adb__send_quit(
        adb__sync_t *sync) {
    (void)adb__send_request(sync, ADB__SYNC_ID_QUIT, "");
}

/* docs later: sync data header and payload can be spread across WRTN */
static adb_error_t adb__pull_v1(
        adb_conn_t *conn,
        const char *remote_path,
        const adb_write_fn write_fn,
        void *userdata)
{
    adb_error_t res = ADB_ERR_OK;

    adb__sync_t sync = {0};
    bool data_done = false;

    /* vars to track header and data across WRTE */
    size_t data_remaining = 0;
    size_t header_written = 0;
    adb__sync_data_t header = {0};

    ADB__INFO("pulling file from device at \"%s\"", remote_path);

    res = adb__sync_proto_open(&sync, conn);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__send_request(&sync, ADB__SYNC_ID_RECV_V1, remote_path);
    if(res != ADB_ERR_OK)
        goto cleanup;
   
    while(!data_done)
    {
        uint8_t *pkt_payload = NULL;
        size_t payload_size = 0;
        size_t pkt_offset = 0;

        res = adb__sync_proto_read(&sync);
        if(res != ADB_ERR_OK)
            goto cleanup;

        if(sync.pkt.command == ADB__CMD_OKAY)
        {
            /* possible error: PROTOCOL */
            res = adb__proto_sync_handle_okay(&sync);
            if(res != ADB_ERR_OK)
                goto cleanup;
            continue;
        }

        pkt_payload = sync.pkt_payload;
        payload_size = sync.pkt.payload_size;
        while(pkt_offset < payload_size && !data_done)
        {
            size_t pkt_remaining = 0;
            size_t available = 0;
            size_t header_remaining = 0;

            pkt_remaining = payload_size - pkt_offset;

            /* The data may be split across multiple WRTE packets. */
            if(data_remaining > 0)
            {
                int write_res = 0;
                available = ADB__MIN(pkt_remaining, data_remaining);
                write_res = write_fn(userdata, 
                        pkt_payload + pkt_offset, available);
                if(write_res < 0)
                {
                    res = (adb_error_t)-write_res;
                    goto cleanup;
                }

                if((size_t)write_res != available)
                {
                    res = ADB_ERR_IO;
                    goto cleanup;
                }

                pkt_offset += available;
                data_remaining -= available;
                continue;
            }

            /* The header may be split across multiple WRTE packets. */
            header_remaining = sizeof(header) - header_written;
            available = ADB__MIN(pkt_remaining, header_remaining);

            memcpy((uint8_t*)&header + header_written,
                    pkt_payload + pkt_offset,
                    available);

            pkt_offset += available;
            header_written += available;

            if(header_written != sizeof(header))
                continue;

            header_written = 0;
            if(header.id == ADB__SYNC_ID_DONE)
            {
                if(header.size != 0)
                {
                    ADB__ERROR("unexpected data payload");
                    ADB__INFO("expected: 0 bytes, got: %u bytes",
                            header.size);
                    res = ADB_ERR_PROTOCOL;
                    goto cleanup;
                }
    
                /* 
                 * although named send ready, it just means 
                 * if we have recieved OKAY, intended for push
                 */
                if(!sync.send_ready)
                    { res = ADB_ERR_PROTOCOL; goto cleanup; }

                data_done = true;
                break;
            }

            if(header.id != ADB__SYNC_ID_DATA)
            {
                ADB__ERROR("unexpected data id");
                ADB__INFO(
                        "expected: 0x%08X (%.4s), got: 0x%08X (%.4s)",
                        (uint32_t)ADB__SYNC_ID_DATA,
                        (uint8_t*)(uint32_t[1]){ADB__SYNC_ID_DATA},
                        header.id,
                        (uint8_t*)&header.id);

                res = ADB_ERR_PROTOCOL;
                goto cleanup;
            }

            if(header.size > ADB__SYNC_DATA_MAX)
            {
                ADB__ERROR("data size exceeds the maximum allowed size");
                ADB__INFO(
                        "max size: %d bytes, got: %u bytes",
                        ADB__SYNC_DATA_MAX,
                        header.size);

                res = ADB_ERR_PROTOCOL;
                goto cleanup;
            }

            data_remaining = header.size;
        }

        res = adb__proto_sync_ack(&sync);
        if(res != ADB_ERR_OK)
            goto cleanup;
    }

    adb__send_quit(&sync);
    ADB__INFO("pulled file from device successfully");

cleanup:
    adb__sync_proto_close(&sync);
    return res;
}

/* docs later: sync data header and payload can be spread across WRTN */
static adb_error_t adb__pull_v2(
        adb_conn_t *conn,
        const char *remote_path,
        const adb__decomp_type_t decomp_type,
        const adb_write_fn write_fn,
        void *userdata)
{
    static uint32_t decomp_type_to_flags[ADB__DECOMP_COUNT] =
    {
        [ADB__DECOMP_NONE] = 0,
        [ADB__DECOMP_BROTLI] = ADB__SYNC_FLAG_BROTLI,
        [ADB__DECOMP_LZ4] = ADB__SYNC_FLAG_LZ4,
        [ADB__DECOMP_ZSTD] = ADB__SYNC_FLAG_ZSTD,
    };

    adb_error_t res = ADB_ERR_OK;
    adb__sync_t sync = {0};
    adb__decomp_t *decomp = NULL;

    bool data_done = false;
    size_t data_remaining = 0;
    size_t header_written = 0;
    adb__sync_data_t header = {0};

    ADB__INFO("pulling file from device at \"%s\"", remote_path);

    res = adb__sync_proto_open(&sync, conn);
    if(res != ADB_ERR_OK)
        return res;
   
    res = adb__send_recv_v2(&sync, remote_path, 
            decomp_type_to_flags[decomp_type]);
    if(res != ADB_ERR_OK)
        return res;
   
    res = adb__decomp_create(&decomp, decomp_type, write_fn, userdata);
    if(res != ADB_ERR_OK)
        goto cleanup;

    while(!data_done)
    {
        uint8_t *pkt_payload = NULL;
        size_t payload_size = 0;
        size_t pkt_offset = 0;

        res = adb__sync_proto_read(&sync);
        if(res != ADB_ERR_OK)
            goto cleanup;

        if(sync.pkt.command == ADB__CMD_OKAY)
        {
            /* possible error: PROTOCOL */
            res = adb__proto_sync_handle_okay(&sync);
            if(res != ADB_ERR_OK)
                goto cleanup;
            continue;
        }

        if(adb__packet_check_cmd(&sync.pkt, ADB__CMD_WRTE))
        {
            res = ADB_ERR_PROTOCOL;
            goto cleanup;
        }

        pkt_payload = sync.pkt_payload;
        payload_size = sync.pkt.payload_size;
        while(pkt_offset < payload_size && !data_done)
        {
            size_t pkt_remaining = 0;
            size_t available = 0;
            size_t header_remaining = 0;

            pkt_remaining = payload_size - pkt_offset;

            /* The data may be split across multiple WRTE packets. */
            if(data_remaining > 0)
            {
                available = ADB__MIN(pkt_remaining, data_remaining);
                res = adb__decomp_decompress(decomp, 
                        pkt_payload + pkt_offset, available);
                if(res != ADB_ERR_OK)
                    goto cleanup;

                pkt_offset += available;
                data_remaining -= available;
                continue;
            }

            /* The header may be split across multiple WRTE packets. */
            header_remaining = sizeof(header) - header_written;
            available = ADB__MIN(pkt_remaining, header_remaining);

            memcpy((uint8_t*)&header + header_written,
                    pkt_payload + pkt_offset,
                    available);

            pkt_offset += available;
            header_written += available;
            if(header_written != sizeof(header))
                continue;

            header_written = 0;
            if(header.id == ADB__SYNC_ID_DONE)
            {
                if(header.size != 0)
                {
                    ADB__ERROR("unexpected data payload");
                    ADB__INFO("expected: 0 bytes, got: %u bytes",
                            header.size);
                    res = ADB_ERR_PROTOCOL;
                    goto cleanup;
                }
    
                /* 
                 * although named send ready, it just means 
                 * if we have recieved OKAY, intended for push
                 */
                if(!sync.send_ready)
                    { res = ADB_ERR_PROTOCOL; goto cleanup; }

                data_done = true;
                break;
            }

            if(header.id != ADB__SYNC_ID_DATA)
            {
                ADB__ERROR("unexpected data id");
                ADB__INFO(
                        "expected: 0x%08X (%.4s), got: 0x%08X (%.4s)",
                        (uint32_t)ADB__SYNC_ID_DATA,
                        (uint8_t*)(uint32_t[1]){ADB__SYNC_ID_DATA},
                        header.id,
                        (uint8_t*)&header.id);

                res = ADB_ERR_PROTOCOL;
                goto cleanup;
            }

            if(header.size > ADB__SYNC_DATA_MAX)
            {
                ADB__ERROR("data size exceeds the maximum allowed size");
                ADB__INFO(
                        "max size: %d bytes, got: %u bytes",
                        ADB__SYNC_DATA_MAX,
                        header.size);

                res = ADB_ERR_PROTOCOL;
                goto cleanup;
            }

            data_remaining = header.size;
        }

        res = adb__proto_sync_ack(&sync);
        if(res != ADB_ERR_OK)
        {
            adb__log_err_adb(res, "failed to send acknowledgement signal");
            goto cleanup;
        }
    }

    ADB__INFO("pulled file from device successfully");

cleanup:
    adb__sync_proto_close(&sync);
    adb__decomp_destroy(decomp);
    return res;
}

adb_error_t adb_sync_pull(
        adb_conn_t *conn,
        const char *remote_path,
        const adb_write_fn write_fn,
        void *userdata)
{
    struct {
        const char *feature;
        adb__decomp_type_t decomp_type;
    } check_orders[ADB__DECOMP_COUNT] =
    {
        { ADB__FEATURE_SENDRECV_V2_ZSTD, ADB__DECOMP_ZSTD },
        { ADB__FEATURE_SENDRECV_V2_LZ4, ADB__DECOMP_LZ4 },
        { ADB__FEATURE_SENDRECV_V2_BROTLI, ADB__DECOMP_BROTLI },
        { ADB__FEATURE_SENDRECV_V2, ADB__DECOMP_NONE },
    };

    if(!conn || !remote_path || !write_fn)
        return ADB_ERR_PARAM;

    return adb__pull_v1(conn, remote_path, write_fn, userdata);   
    for(size_t i = 0; i < ADB__ARRSZ(check_orders); i++)
    {
        /* XXX: dunno it needs to try another if current failed */
        if(adb__has_feature(conn, check_orders[i].feature))
        {
            return adb__pull_v2(conn, remote_path, 
                    check_orders[i].decomp_type, 
                    write_fn, userdata);
        }
    }

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
        return ADB_ERR_IO;

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
            ADB__PATH_MAX +
            sizeof(msg)] = {0};
    uint8_t *req_cursor = NULL;

    if(!sync || !path)
        return ADB_ERR_PARAM;

    path_len = strlen(path);
    if(path_len > ADB__PATH_MAX)
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
    adb__sync_t sync = {0};
    adb__comp_t *comp = NULL;
    size_t remaining = 0;
    uint8_t in_buf[ADB__PUSH_IN_BUFFER_SIZE] = {0};
    if(!conn || !remote_path || !read_fn)
        return ADB_ERR_PARAM;
    
    res = adb__sync_proto_open(&sync, conn);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__send_send_v2(&sync, remote_path, mode, 0);
    if(res != ADB_ERR_OK)
        return res;

    res = adb__comp_create(&comp, ADB__COMP_NONE);
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

                res = adb__proto_sync_check_ack(&sync);
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
                
            res = adb__proto_sync_check_ack(&sync);
            if(res != ADB_ERR_OK)
                goto cleanup;
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

    adb__send_quit(&sync);

cleanup:
    adb__sync_proto_close(&sync);
    return res;
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

    stat(local_path, &st);
    // TODO check stat err
     
    file = fopen(local_path, "rb");
    if(!file)
        return ADB_ERR_IO;

    res = adb_sync_push(conn, remote_path, 
            st.st_mode, (uint32_t)st.st_mtim.tv_sec, 
            (size_t)st.st_size,
            adb__push_file_read, file);
    fclose(file);
    return res;
}

