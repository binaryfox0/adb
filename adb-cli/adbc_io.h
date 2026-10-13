#ifndef ADBC_IO_H
#define ADBC_IO_H

#include <stddef.h>
#include <stdbool.h>

typedef struct yyjson_doc yyjson_doc;
typedef struct yyjson_val yyjson_val;
typedef struct yyjson_mut_doc yyjson_mut_doc;

bool adbc_send_json(int fd, yyjson_mut_doc *doc);
bool adbc_send_json_raw(int fd, const char *json);
bool adbc_recv_json(int fd, yyjson_doc **out_doc);

#endif
