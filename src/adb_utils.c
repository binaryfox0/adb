#include "adb_utils.h"

#include <stdio.h>
#include "adb_alloc_priv.h"

adb_error_t adb__util_read_file(
        const char *path,
        char **buf_out,
        size_t *size_out)
{
    FILE *file = NULL;
    size_t file_size = 0;
    char *tmp = NULL;
    if(!path || !buf_out || !size_out)
        return ADB_ERR_PARAM;

    file = fopen(path, "r");
    if(!file)
        return ADB_ERR_IO;

    fseek(file, 0, SEEK_END);
    file_size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    tmp = adb__malloc(file_size + 1);
    if(!tmp)
    {
        fclose(file);
        return ADB_ERR_NO_MEM;
    }

    if(fread(tmp, 1, file_size, 
                file) != file_size)
    {
        adb__free(tmp);
        fclose(file);
        return ADB_ERR_IO;
    }
    tmp[file_size] = '\0';

    fclose(file); 

    *buf_out = tmp;
    *size_out = file_size;
    return ADB_ERR_OK;
}

adb_error_t adb__util_write_file(
        const char *path,
        const uint8_t *buf,
        const size_t size)
{
    FILE *file = NULL;
    if(!path || !buf || size == 0)
        return ADB_ERR_PARAM;

    file = fopen(path, "w");
    if(!file)
        return ADB_ERR_IO;

    if(fwrite(buf, 1, size, file) != size)
    {
        fclose(file);
        return ADB_ERR_IO;
    }
    
    fclose(file);
    return ADB_ERR_OK;
}
