
#include "adb_comp.h"

#include <stdbool.h>
#include <stdint.h>

#include "adb_utils.h"
#include "adb_alloc_priv.h"
#include "adb_log_priv.h"

#include <lz4frame.h>
#include <zstd.h>
#include <brotli/encode.h>

#define ADB__COMP_BUFFER_SIZE (128 * 1024)
#define ADB__COMP_INPUT_SIZE  (64 * 1024)

#define ADB__COMP_BROTLI_QUALITY 1
#define ADB__COMP_ZSTD_LEVEL     1

typedef struct
{
    LZ4F_cctx *ctx;
    bool started;
} adb__comp_lz4_t;

typedef struct
{
    ZSTD_CStream *stream;
} adb__comp_zstd_t;

typedef struct
{
    BrotliEncoderState *state;
} adb__comp_brotli_t;

typedef struct adb__comp
{
    adb__comp_type_t type;
    adb_write_fn write_fn;
    void *userdata;
    bool done;
    union
    {
        adb__comp_lz4_t lz4;
        adb__comp_zstd_t zstd;
        adb__comp_brotli_t brotli;
    };
} adb__comp_t;

typedef adb_error_t (*adb__comp_compress_fn)(
        adb__comp_t *comp,
        const void *data,
        const size_t size);

typedef adb_error_t (*adb__comp_finish_fn)(
        adb__comp_t *comp);

typedef size_t (*adb__comp_input_bound_fn)(
        const size_t out_max);


static adb_error_t adb__comp_write(
        adb__comp_t *comp,
        const void *data,
        const size_t size)
{
    int write_res = 0;

    if(size == 0)
        return ADB_ERR_OK;

    write_res = comp->write_fn(comp->userdata, data, size);
    if(write_res < 0)
        return (adb_error_t)-write_res;

    if((size_t)write_res != size)
        return ADB_ERR_IO;

    return ADB_ERR_OK;
}


static adb_error_t adb__comp_brotli_init(
        adb__comp_brotli_t *brotli)
{
    BrotliEncoderState *state = NULL;
    BROTLI_BOOL res = BROTLI_FALSE;

    if(!brotli)
        return ADB_ERR_PARAM;

    state = BrotliEncoderCreateInstance(
            adb__alloc_get()->malloc,
            adb__alloc_get()->free,
            adb__alloc_get()->userdata);

    if(!state)
    {
        ADB__ERROR("failed to create brotli compressor");
        ADB__INFO("reason: out of memory");
        return ADB_ERR_NO_MEM;
    }

    res = BrotliEncoderSetParameter(
            state,
            BROTLI_PARAM_QUALITY,
            ADB__COMP_BROTLI_QUALITY);

    if(res == BROTLI_FALSE)
    {
        ADB__ERROR("failed to configure brotli compressor");
        BrotliEncoderDestroyInstance(state);
        return ADB_ERR_COMPRESS;
    }

    brotli->state = state;
    return ADB_ERR_OK;
}


static adb_error_t adb__comp_lz4_init(
        adb__comp_lz4_t *lz4)
{
    LZ4F_cctx *ctx = NULL;

    if(!lz4)
        return ADB_ERR_PARAM;

    ctx = LZ4F_createCompressionContext_advanced(
            (LZ4F_CustomMem){
                .customAlloc = adb__alloc_get()->malloc,
                .customCalloc = NULL,
                .customFree = adb__alloc_get()->free,
                .opaqueState = adb__alloc_get()->userdata
            },
            LZ4F_VERSION);

    if(!ctx)
    {
        ADB__ERROR("failed to create lz4 compressor");
        ADB__INFO("reason: out of memory");
        return ADB_ERR_NO_MEM;
    }

    lz4->ctx = ctx;
    lz4->started = false;

    return ADB_ERR_OK;
}


static adb_error_t adb__comp_zstd_init(
        adb__comp_zstd_t *zstd)
{
    size_t err = 0;
    ZSTD_CStream *stream = NULL;

    if(!zstd)
        return ADB_ERR_PARAM;

    stream = ZSTD_createCStream_advanced(
            (ZSTD_customMem){
                .customAlloc = adb__alloc_get()->malloc,
                .customFree = adb__alloc_get()->free,
                .opaque = adb__alloc_get()->userdata
            });

    if(!stream)
    {
        ADB__ERROR("failed to create zstd compressor");
        ADB__INFO("reason: out of memory");
        return ADB_ERR_NO_MEM;
    }

    err = ZSTD_CCtx_setParameter(
            stream,
            ZSTD_c_compressionLevel,
            ADB__COMP_ZSTD_LEVEL);

    if(ZSTD_isError(err))
    {
        ADB__ERROR("failed to configure zstd compressor");
        ADB__INFO("reason: %s", ZSTD_getErrorName(err));
        ZSTD_freeCStream(stream);
        return ADB_ERR_COMPRESS;
    }

    zstd->stream = stream;

    return ADB_ERR_OK;
}


adb_error_t adb__comp_create(
        adb__comp_t **comp,
        const adb__comp_type_t type,
        const adb_write_fn write_fn,
        void *userdata)
{
    adb__comp_t *tmp = NULL;
    adb_error_t ret = ADB_ERR_OK;

    if(!comp || !ADB__CHECK_ENUM(type, COMP) || !write_fn)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    if(type == ADB__COMP_LZ4)
        ret = adb__comp_lz4_init(&tmp->lz4);
    else if(type == ADB__COMP_ZSTD)
        ret = adb__comp_zstd_init(&tmp->zstd);
    else if(type == ADB__COMP_BROTLI)
        ret = adb__comp_brotli_init(&tmp->brotli);

    if(ret != ADB_ERR_OK)
    {
        adb__free(tmp);
        return ret;
    }

    tmp->type = type;
    tmp->write_fn = write_fn;
    tmp->userdata = userdata;
    *comp = tmp;

    return ADB_ERR_OK;
}


static adb_error_t adb__comp_compress_none(
        adb__comp_t *comp,
        const void *data,
        const size_t size)
{
    (void)comp;
    return adb__comp_write(comp, data, size);
}


static adb_error_t adb__comp_finish_none(
        adb__comp_t *comp)
{
    (void)comp;
    return ADB_ERR_OK;
}


static adb_error_t adb__comp_compress_lz4(
        adb__comp_t *comp,
        const void *data,
        const size_t size)
{
    adb_error_t res = ADB_ERR_OK;
    size_t offset = 0;
    uint8_t buffer[ADB__COMP_BUFFER_SIZE] = {0};
    size_t header_size = 0;
    size_t compressed_size = 0;
    size_t chunk_size = 0;

    if(!comp->lz4.started)
    {
        header_size = LZ4F_compressBegin(
                comp->lz4.ctx,
                buffer,
                sizeof(buffer),
                NULL);

        if(LZ4F_isError(header_size))
        {
            ADB__ERROR("failed to start lz4 compression");
            ADB__INFO("reason: %s",
                    LZ4F_getErrorName(header_size));
            return ADB_ERR_COMPRESS;
        }

        compressed_size = header_size;

        res = adb__comp_write(comp, buffer, compressed_size);
        if(res != ADB_ERR_OK)
            return res;

        comp->lz4.started = true;
    }

    while(offset < size)
    {
        chunk_size = size - offset;
        if(chunk_size > ADB__COMP_INPUT_SIZE)
            chunk_size = ADB__COMP_INPUT_SIZE;

        compressed_size = LZ4F_compressUpdate(
                comp->lz4.ctx,
                buffer,
                sizeof(buffer),
                (const uint8_t *)data + offset,
                chunk_size,
                NULL);

        if(LZ4F_isError(compressed_size))
        {
            ADB__ERROR("failed to compress lz4 data");
            ADB__INFO("reason: %s",
                    LZ4F_getErrorName(compressed_size));
            return ADB_ERR_COMPRESS;
        }

        offset += chunk_size;

        if(compressed_size != 0)
        {
            res = adb__comp_write(comp, buffer, compressed_size);
            if(res != ADB_ERR_OK)
                return res;
        }
    }

    return ADB_ERR_OK;
}


static adb_error_t adb__comp_finish_lz4(
        adb__comp_t *comp)
{
    adb_error_t res = ADB_ERR_OK;
    size_t compressed_size = 0;
    uint8_t buffer[ADB__COMP_BUFFER_SIZE] = {0};

    if(!comp->lz4.started)
    {
        compressed_size = LZ4F_compressBegin(
                comp->lz4.ctx,
                buffer,
                sizeof(buffer),
                NULL);

        if(LZ4F_isError(compressed_size))
        {
            ADB__ERROR("failed to start lz4 compression");
            ADB__INFO("reason: %s",
                    LZ4F_getErrorName(compressed_size));
            return ADB_ERR_COMPRESS;
        }

        res = adb__comp_write(comp, buffer, compressed_size);
        if(res != ADB_ERR_OK)
            return res;

        comp->lz4.started = true;
    }

    compressed_size = LZ4F_compressEnd(
            comp->lz4.ctx,
            buffer,
            sizeof(buffer),
            NULL);

    if(LZ4F_isError(compressed_size))
    {
        ADB__ERROR("failed to finish lz4 compression");
        ADB__INFO("reason: %s",
                LZ4F_getErrorName(compressed_size));
        return ADB_ERR_COMPRESS;
    }

    return adb__comp_write(comp, buffer, compressed_size);
}


static adb_error_t adb__comp_compress_zstd(
        adb__comp_t *comp,
        const void *data,
        const size_t size)
{
    ZSTD_inBuffer in = {0};
    size_t err = 0;
    size_t old_in_pos = 0;
    size_t old_out_pos = 0;
    uint8_t buffer[ADB__COMP_BUFFER_SIZE] = {0};

    in.src = data;
    in.size = size;
    in.pos = 0;

    while(in.pos < in.size)
    {
        ZSTD_outBuffer out = {0};
        out.dst = buffer;
        out.size = sizeof(buffer);

        old_in_pos = in.pos;
        old_out_pos = out.pos;

        err = ZSTD_compressStream2(
                comp->zstd.stream,
                &out,
                &in,
                ZSTD_e_continue);

        if(ZSTD_isError(err))
        {
            ADB__ERROR("failed to compress zstd data");
            ADB__INFO("reason: %s", ZSTD_getErrorName(err));
            return ADB_ERR_COMPRESS;
        }

        if(out.pos != 0)
        {
            adb_error_t res = adb__comp_write(comp, buffer, out.pos);
            if(res != ADB_ERR_OK)
                return res;
        }

        if(in.pos == old_in_pos && out.pos == old_out_pos)
            return ADB_ERR_COMPRESS;
    }

    return ADB_ERR_OK;
}


static adb_error_t adb__comp_finish_zstd(
        adb__comp_t *comp)
{
    ZSTD_inBuffer in = {0};
    size_t err = 0;
    size_t old_in_pos = 0;

    while(err != 0)
    {
        uint8_t buffer[ADB__COMP_BUFFER_SIZE] = {0};
        ZSTD_outBuffer out = {0};

        out.dst = buffer;
        out.size = sizeof(buffer);

        old_in_pos = in.pos;
        err = ZSTD_compressStream2(
                comp->zstd.stream,
                &out,
                &in,
                ZSTD_e_end);

        if(ZSTD_isError(err))
        {
            ADB__ERROR("failed to finish zstd compression");
            ADB__INFO("reason: %s", ZSTD_getErrorName(err));
            return ADB_ERR_COMPRESS;
        }

        if(out.pos != 0)
        {
            adb_error_t res = adb__comp_write(comp, buffer, out.pos);
            if(res != ADB_ERR_OK)
                return res;
        }

        if(err != 0 && in.pos == old_in_pos && out.pos == 0)
            return ADB_ERR_COMPRESS;
    }

    return ADB_ERR_OK;
}


static adb_error_t adb__comp_compress_brotli(
        adb__comp_t *comp,
        const void *data,
        const size_t size)
{
    size_t avail_in = size;
    size_t avail_out = 0;
    size_t produced = 0;
    const uint8_t *input = NULL;
    uint8_t *output = NULL;
    uint8_t buffer[ADB__COMP_BUFFER_SIZE] = {0};

    input = (const uint8_t *)data;
    while(avail_in != 0)
    {
        avail_out = sizeof(buffer);
        output = buffer;

        if(BrotliEncoderCompressStream(
                    comp->brotli.state,
                    BROTLI_OPERATION_PROCESS,
                    &avail_in,
                    &input,
                    &avail_out,
                    &output,
                    NULL) == BROTLI_FALSE)
        {
            ADB__ERROR("failed to compress brotli data");
            return ADB_ERR_COMPRESS;
        }

        produced = sizeof(buffer) - avail_out;
        if(produced != 0)
        {
            adb_error_t res = adb__comp_write(comp, buffer, produced);
            if(res != ADB_ERR_OK)
                return res;
        }

        if(avail_in != 0 && produced == 0)
            return ADB_ERR_COMPRESS;
    }

    return ADB_ERR_OK;
}


static adb_error_t adb__comp_finish_brotli(
        adb__comp_t *comp)
{
    size_t avail_in = 0;
    size_t avail_out = 0;
    size_t produced = 0;
    const uint8_t *input = NULL;
    uint8_t *output = NULL;
    uint8_t buffer[ADB__COMP_BUFFER_SIZE] = {0};

    for(;;)
    {
        if(BrotliEncoderIsFinished(comp->brotli.state))
            break;

        avail_out = sizeof(buffer);
        output = buffer;

        if(BrotliEncoderCompressStream(
                    comp->brotli.state,
                    BROTLI_OPERATION_FINISH,
                    &avail_in,
                    &input,
                    &avail_out,
                    &output,
                    NULL) == BROTLI_FALSE)
        {
            ADB__ERROR("failed to finish brotli compression");
            return ADB_ERR_COMPRESS;
        }

        produced = sizeof(buffer) - avail_out;
        if(produced != 0)
        {
            adb_error_t res = adb__comp_write(comp, buffer, produced);
            if(res != ADB_ERR_OK)
                return res;
        }

        if(produced == 0 &&
                !BrotliEncoderIsFinished(comp->brotli.state))
            return ADB_ERR_COMPRESS;
    }

    return ADB_ERR_OK;
}


adb_error_t adb__comp_compress(
        adb__comp_t *comp,
        const void *data,
        const size_t size)
{
    static const adb__comp_compress_fn funcs[ADB__COMP_COUNT] =
    {
        [ADB__COMP_NONE]   = adb__comp_compress_none,
        [ADB__COMP_BROTLI] = adb__comp_compress_brotli,
        [ADB__COMP_LZ4]    = adb__comp_compress_lz4,
        [ADB__COMP_ZSTD]   = adb__comp_compress_zstd
    };

    adb__comp_compress_fn fn = NULL;
    if(!comp || !data || size == 0)
        return ADB_ERR_PARAM;

    if(comp->done)
        return ADB_ERR_PROTOCOL;

    fn = funcs[comp->type];
    if(!fn)
        return ADB_ERR_UNSUPPORTED;

    return fn(comp, data, size);
}


adb_error_t adb__comp_finish(
        adb__comp_t *comp)
{
    static const adb__comp_finish_fn funcs[ADB__COMP_COUNT] =
    {
        [ADB__COMP_NONE]   = adb__comp_finish_none,
        [ADB__COMP_BROTLI] = adb__comp_finish_brotli,
        [ADB__COMP_LZ4]    = adb__comp_finish_lz4,
        [ADB__COMP_ZSTD]   = adb__comp_finish_zstd
    };

    adb__comp_finish_fn fn = NULL;
    adb_error_t ret = ADB_ERR_OK;
    if(!comp)
        return ADB_ERR_PARAM;
    if(comp->done)
        return ADB_ERR_PROTOCOL;

    fn = funcs[comp->type];
    if(!fn)
        return ADB_ERR_UNSUPPORTED;

    ret = fn(comp);
    if(ret != ADB_ERR_OK)
        return ret;

    comp->done = true;
    return ADB_ERR_OK;
}


void adb__comp_destroy(
        adb__comp_t *comp)
{
    if(!comp)
        return;

    if(comp->type == ADB__COMP_LZ4)
        LZ4F_freeCompressionContext(comp->lz4.ctx);
    else if(comp->type == ADB__COMP_ZSTD)
        ZSTD_freeCStream(comp->zstd.stream);
    else if(comp->type == ADB__COMP_BROTLI)
        BrotliEncoderDestroyInstance(comp->brotli.state);

    adb__free(comp);
}

static size_t adb__comp_input_bound_none(
        const size_t out_max)
{
    return out_max;
}

static size_t adb__comp_input_bound_lz4(
        const size_t out_max)
{
    size_t lo = 0;
    size_t hi = out_max;
    while (lo < hi) 
    {
        size_t mid = lo + (hi - lo + 1) / 2;
        if (LZ4F_compressBound(mid, NULL) <= out_max)
            lo = mid;
        else
            hi = mid - 1;
    }

    return lo;
}

static size_t adb__comp_input_bound_zstd(
        const size_t out_max)
{
    size_t lo = 0;
    size_t hi = out_max;
    while (lo < hi) 
    {
        size_t mid = lo + (hi - lo + 1) / 2;
        if (ZSTD_compressBound(mid) <= out_max)
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo;
}

static size_t adb__comp_input_bound_brotli(
        const size_t out_max)
{
    size_t lo = 0;
    size_t hi = out_max;
    while (lo < hi) 
    {
        size_t mid = lo + (hi - lo + 1) / 2;
        size_t bound = BrotliEncoderMaxCompressedSize(mid);
        if (bound != 0 && bound <= out_max)
            lo = mid;
        else
            hi = mid - 1;
    }

    return lo;
}

size_t adb__comp_input_bound(
        adb__comp_t *comp,
        const size_t out_max)
{
    static const adb__comp_input_bound_fn funcs[ADB__COMP_COUNT] =
    {
        [ADB__COMP_NONE]   = adb__comp_input_bound_none,
        [ADB__COMP_BROTLI] = adb__comp_input_bound_brotli,
        [ADB__COMP_LZ4]    = adb__comp_input_bound_lz4,
        [ADB__COMP_ZSTD]   = adb__comp_input_bound_zstd
    };

    adb__comp_input_bound_fn fn = NULL;
    if(!comp)
        return 0;

    fn = funcs[comp->type];
    if(!fn)
        return 0;

    return fn(out_max);
}

