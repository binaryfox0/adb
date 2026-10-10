#include "adbc_utils.h"

#ifdef _WIN32
#   include <windows.h>
#else
#   include <arpa/inet.h>
#   include <stdlib.h>
#   include <pwd.h>
#   include <unistd.h>
#   include <time.h>
#endif

const char *adbc_util_home_dir(void)
{
    const char *home = NULL;
    struct passwd *password_entry = NULL;

    home = getenv("HOME");
    if(home != NULL && home[0] != '\0')
        return home;

    password_entry = getpwuid(getuid());
    if(password_entry != NULL &&
            password_entry->pw_dir != NULL &&
            password_entry->pw_dir[0] != '\0')
        return password_entry->pw_dir;

    return NULL;
}

bool adbc_util_parse_endpoint(
        const char *input,
        char *out_host,
        size_t size,
        uint16_t *out_port)
{
    const char *host_begin;
    const char *host_end;
    const char *port_begin;
    const char *p;
    unsigned long port = 0;
    size_t host_len;

    if(!input || !*input || !out_host || !size || !out_port)
        return false;

    if(input[0] == '[')
    {
        /* IPv6: [2001:db8::1]:5555 */
        host_begin = input + 1;
        host_end = strchr(host_begin, ']');
        if(!host_end || host_end[1] != ':')
            return false;

        port_begin = host_end + 2;
        if(!*port_begin)
            return false;

        host_len = (size_t)(host_end - host_begin);
        if(!host_len || host_len >= INET6_ADDRSTRLEN)
            return false;
    }
    else
    {
        /* IPv4: 192.168.1.10:5555 */
        host_begin = input;
        host_end = strchr(input, ':');
        if(!host_end || host_end == host_begin)
            return false;

        port_begin = host_end + 1;
        if(!*port_begin)
            return false;

        host_len = (size_t)(host_end - host_begin);
        if(host_len >= INET_ADDRSTRLEN)
            return false;

        /* Unbracketed IPv6 is not supported. */
        if(strchr(port_begin, ':'))
            return false;
    }

    for(p = port_begin; *p; ++p)
    {
        if(*p < '0' || *p > '9')
            return false;

        port = port * 10UL + (unsigned long)(*p - '0');
        if(port > UINT16_MAX)
            return false;
    }

    if(size <= host_len)
        return false;

    memcpy(out_host, host_begin, host_len);
    out_host[host_len] = '\0';
    *out_port = (uint16_t)port;

    return true;
}

bool adbc_util_file_exist(
        const char *path)
{
    FILE *file = fopen(path, "r");
    if(!file)
        return errno == ENOENT ? false : true;
    fclose(file);
    return true;
}

#if defined(_WIN32)
uint64_t adbc_util_monotonic_ms(void) {
    return (uint64_t)GetTickCount64();
}
#else
uint64_t adbc_util_monotonic_ms(void)
{
    struct timespec timestamp = {0};
    if(clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0)
        return 0;

    return (uint64_t)timestamp.tv_sec * UINT64_C(1000) +
           (uint64_t)timestamp.tv_nsec / UINT64_C(1000000);
}
#endif
