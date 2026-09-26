#include "adb_decomp.h"

#include <stdbool.h>

#include "adb_utils.h"
#include "adb_alloc_priv.h"
#include "adb_log_priv.h"

#include <lz4frame.h>
#include <zstd.h>
#include <brotli/decode.h>

#define ADB__DECOMP_BUFFER_SIZE (64 * 1024)

typedef struct 
{
    LZ4F_dctx *ctx;
} adb__decomp_lz4_t;

typedef struct
{
    ZSTD_DStream *stream;
} adb__decomp_zstd_t;

typedef struct
{
    BrotliDecoderState *state;
} adb__decomp_brotli_t;

typedef struct adb__decomp
{
    adb__decomp_type_t type;
    adb_write_fn write_fn;
    void *userdata;
    bool done;
    union
    {
        adb__decomp_lz4_t lz4;
        adb__decomp_zstd_t zstd;
        adb__decomp_brotli_t brotli;
    };
} adb__decomp_t;

typedef adb_error_t (*adb__decomp_decompress_fn)(
        adb__decomp_t *decomp,
        const void *data,
        const size_t size);

static adb_error_t adb__decomp_brotli_init(
        adb__decomp_brotli_t *brotli)
{
    BrotliDecoderState *state = NULL;
    if(!brotli)
        return ADB_ERR_PARAM;
    state = BrotliDecoderCreateInstance(
            adb__alloc_get()->malloc,
            adb__alloc_get()->free,
            adb__alloc_get()->userdata);

    if(!state)
    {
        ADB__ERROR("failed to create brotli decompressor");
        ADB__INFO("reason: out of memory");
        return ADB_ERR_NO_MEM;
    }

    brotli->state = state;
    return ADB_ERR_OK;
}

static adb_error_t adb__decomp_lz4_init(
        adb__decomp_lz4_t *lz4)
{
    LZ4F_dctx *ctx = NULL;
    if(!lz4)
        return ADB_ERR_PARAM;

    ctx = LZ4F_createDecompressionContext_advanced(
            (LZ4F_CustomMem){
                .customAlloc = adb__alloc_get()->malloc,
                .customFree = adb__alloc_get()->free,
                .opaqueState = adb__alloc_get()->userdata
            },
            LZ4F_VERSION);

    if(!ctx)
    {
        ADB__ERROR("failed to create lz4 decompressor");
        ADB__INFO("reason: out of memory");
        return ADB_ERR_NO_MEM;
    }

    lz4->ctx = ctx;
    return ADB_ERR_OK;
}

static adb_error_t adb__decomp_zstd_init(
        adb__decomp_zstd_t *zstd)
{
    size_t err = 0;
    ZSTD_DStream *stream = NULL;
    if(!zstd)
        return ADB_ERR_PARAM;

    stream = ZSTD_createDStream_advanced(
            (ZSTD_customMem){
                .customAlloc = adb__alloc_get()->malloc,
                .customFree = adb__alloc_get()->free,
                .opaque = adb__alloc_get()->userdata
            });

    if(!stream)
    {
        ADB__ERROR("failed to create zstd decompressor");
        ADB__INFO("reason: out of memory");
        return ADB_ERR_NO_MEM;
    }

    err = ZSTD_initDStream(stream);
    if(ZSTD_isError(err))
    {
        ADB__ERROR("failed to initialize zstd decompressor");
        ADB__INFO("reason: %s", ZSTD_getErrorName(err));
        ZSTD_freeDStream(stream);
        return ADB_ERR_COMPRESS;
    }

    zstd->stream = stream;
    return ADB_ERR_OK;
}


adb_error_t adb__decomp_create(
        adb__decomp_t **decomp,
        const adb__decomp_type_t type,
        const adb_write_fn write_fn,
        void *userdata)
{
    adb__decomp_t *tmp = NULL;
    adb_error_t ret = ADB_ERR_OK;

    if(!decomp || !ADB__CHECK_ENUM(type, DECOMP) || !write_fn)
        return ADB_ERR_PARAM;

    tmp = adb__calloc(1, sizeof(*tmp));
    if(!tmp)
        return ADB_ERR_NO_MEM;

    if(type == ADB__DECOMP_LZ4)
        ret = adb__decomp_lz4_init(&tmp->lz4);
    else if(type == ADB__DECOMP_ZSTD)
        ret = adb__decomp_zstd_init(&tmp->zstd);
    else if(type == ADB__DECOMP_BROTLI)
        ret = adb__decomp_brotli_init(&tmp->brotli);
    
    if(ret != ADB_ERR_OK)
    {
        adb__free(tmp);
        return ret;
    }

    tmp->type = type;
    tmp->write_fn = write_fn;
    tmp->userdata = userdata;
    *decomp = tmp;
    return ADB_ERR_OK;
}

static adb_error_t adb__decomp_decompress_none(
        adb__decomp_t *decomp,
        const void *data,
        const size_t size)
{
    (void)decomp;
    int write_res = decomp->write_fn(
            decomp->userdata, data, size);
    if(write_res < 0)
        return (adb_error_t)-write_res;
    if((size_t)write_res != size)
        return ADB_ERR_IO;
    return ADB_ERR_OK;
}

static adb_error_t adb__decomp_decompress_brotli(
        adb__decomp_t *decomp,
        const void *data,
        const size_t size)
{
    size_t offset = 0;
    while(offset < size) 
    {
        BrotliDecoderResult err = BROTLI_DECODER_RESULT_SUCCESS;
        size_t avail_in = 0;
        size_t avail_out = 0;
        const uint8_t *input = NULL;
        uint8_t *output = NULL;
        uint8_t buffer[ADB__DECOMP_BUFFER_SIZE] = {0};
        size_t consumed = 0;
        size_t produced = 0;

        avail_in = size - offset;
        avail_out = sizeof(buffer);

        input = (const uint8_t*)data + offset;
        output = buffer;

        err = BrotliDecoderDecompressStream(
                decomp->brotli.state,
                &avail_in,
                &input,
                &avail_out,
                &output,
                NULL);

        consumed = (size - offset) - avail_in;
        produced = sizeof(buffer) - avail_out;

        offset += consumed;
        if(produced != 0)
        {
            int write_res = decomp->write_fn(
                    decomp->userdata, buffer, produced);
            if(write_res < 0)
                return (adb_error_t)-write_res;

            if((size_t)write_res != produced)
                return ADB_ERR_IO;
        }

        switch(err) 
        {
            case BROTLI_DECODER_RESULT_SUCCESS:
                decomp->done = true;
                /* One ADB transfer contains one Brotli stream. */
                if (offset != size)
                    return ADB_ERR_COMPRESS;
                return ADB_ERR_OK;

            case BROTLI_DECODER_RESULT_ERROR:
                ADB__ERROR("failed to decompress brotli data");
                ADB__INFO("reason: %s",
                        BrotliDecoderErrorString(
                            BrotliDecoderGetErrorCode(
                                decomp->brotli.state)));
                return ADB_ERR_COMPRESS;

            case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
                /*  The next WRTE will continues the same Brotli stream. */
                if (avail_in != 0)
                    return ADB_ERR_COMPRESS;
                offset = size;
                break;

            case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT:
                if (consumed == 0 && produced == 0)
                    return ADB_ERR_COMPRESS;
                break;

            default:
                return ADB_ERR_GENERIC;
        }
    }
    return ADB_ERR_OK;
}

static adb_error_t adb__decomp_decompress_lz4(
        adb__decomp_t *decomp,
        const void *data,
        const size_t size)
{
    size_t offset = 0;
    while(offset < size)
    {
        size_t consumed = size - offset;
        size_t produced = ADB__DECOMP_BUFFER_SIZE;
        uint8_t buffer[ADB__DECOMP_BUFFER_SIZE] = {0};
        size_t err = 0;

        err = LZ4F_decompress(
                decomp->lz4.ctx,
                buffer,
                &produced,
                (const uint8_t *)data + offset,
                &consumed,
                NULL);

        if(LZ4F_isError(err))
        {
            ADB__ERROR("failed to decompress lz4 data");
            ADB__INFO("reason: %s", LZ4F_getErrorName(err)); 
            return ADB_ERR_COMPRESS;
        }

        offset += consumed;

        /* The decoder must either consume input or produce output. */
        if(consumed == 0 && produced == 0)
            return ADB_ERR_COMPRESS;

        if(produced != 0)
        {
            int write_res = decomp->write_fn(
                    decomp->userdata, buffer, produced);
            if(write_res < 0)
                return (adb_error_t)-write_res;

            if((size_t)write_res != produced)
                return ADB_ERR_IO;
        }

        /* 
         * ADB only use one compression stream for a transfer,
         * any additional bytes will be considered as protocol error 
         */
        if(err == 0)
        {
            decomp->done = true;
            if(offset < size)
                return ADB_ERR_PROTOCOL;
            break;
        }
    }

    return ADB_ERR_OK;
}

static adb_error_t adb__decomp_decompress_zstd(
        adb__decomp_t *decomp,
        const void *data,
        const size_t size)
{
    ZSTD_inBuffer in = {0};

    in.src = data;
    in.size = size;
    while(in.pos < in.size) 
    {
        size_t err = 0;
        uint8_t buffer[ADB__DECOMP_BUFFER_SIZE] = {0};
        ZSTD_outBuffer out = {0};

        out.dst = buffer;
        out.size = sizeof(buffer);
        out.pos = 0;

        err = ZSTD_decompressStream(
                decomp->zstd.stream,
                &out,
                &in);

        if (ZSTD_isError(err) != 0)
        {
            ADB__ERROR("failed to decompress zstd data");
            ADB__INFO("reason: %s", ZSTD_getErrorName(err)); 
            return ADB_ERR_COMPRESS;
        }

        if (out.pos != 0) 
        {
            int write_res = decomp->write_fn(
                    decomp->userdata, buffer, out.pos);
            if(write_res < 0)
                return (adb_error_t)-write_res;

            if((size_t)write_res != out.pos)
                return ADB_ERR_IO;
        }

        if(err == 0) 
        {
            decomp->done = true;
            if(in.pos != in.size)
                return ADB_ERR_COMPRESS;
            break;
        }

        if (out.pos == 0 && in.pos == 0 && in.size != 0)
            return ADB_ERR_COMPRESS;
    }

    return ADB_ERR_OK;
}

adb_error_t adb__decomp_decompress(
        adb__decomp_t *decomp,
        const void *data,
        const size_t size)
{
    static const adb__decomp_decompress_fn funcs[ADB__DECOMP_COUNT] =
    {
        [ADB__DECOMP_NONE]   = adb__decomp_decompress_none,
        [ADB__DECOMP_BROTLI] = adb__decomp_decompress_brotli,
        [ADB__DECOMP_LZ4]    = adb__decomp_decompress_lz4,
        [ADB__DECOMP_ZSTD]   = adb__decomp_decompress_zstd
    };
    adb__decomp_decompress_fn fn = NULL;
    if(!decomp || !data || size == 0)
        return ADB_ERR_PARAM;
    if(decomp->done)
        return ADB_ERR_PROTOCOL;

    fn = funcs[decomp->type];
    if(!fn)
        return ADB_ERR_UNSUPPORTED;
    return fn(decomp, data, size);
}

void adb__decomp_destroy(
        adb__decomp_t *decomp)
{
    if(!decomp)
        return;
    
    if(decomp->type == ADB__DECOMP_LZ4)
        LZ4F_freeDecompressionContext(decomp->lz4.ctx);
    else if(decomp->type == ADB__DECOMP_ZSTD)
        ZSTD_freeDStream(decomp->zstd.stream);
    else if(decomp->type == ADB__DECOMP_BROTLI)
        BrotliDecoderDestroyInstance(decomp->brotli.state);
    adb__free(decomp);
}
