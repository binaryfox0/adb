#include <adb/adb_stream.h>

#include "adb_stream_priv.h"

bool adb_stream_done(
		const adb_stream_t *stream)
{
	return stream ? stream->done : true;
}

adb_error_t adb_stream_error(
		const adb_stream_t *stream)
{
	return stream ? stream->error : ADB_ERR_PARAM;
}
