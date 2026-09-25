#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <pwd.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <aparse.h>
#include <adb/adb.h>
#include <qrcodegen.h>

#define error aparse_prog_error
#define info aparse_prog_info

#define CHECK(x, res, label, ...) \
    if(((res) = x) != ADB_ERR_OK) \
    { \
        error(__VA_ARGS__); \
        info("reason: %s", adb_strerror((res))); \
        goto label; \
    }

static void log_callback(
        void *userdata,
        adb_log_level_t level,
        const char *msg)
{
    (void)userdata;
    switch(level)
    {
        case ADB_LOG_DEBUG: 
            aparse_log("adb", APARSE__DEBUG_LABEL, "%s", msg);
            break;

        case ADB_LOG_INFO:
            aparse_log("adb", APARSE__INFO_LABEL, "%s", msg);
            break;

        case ADB_LOG_WARN:
            aparse_log("adb", APARSE__WARN_LABEL, "%s", msg);
            break;

        case ADB_LOG_ERROR:
            aparse_log("adb", APARSE__ERROR_LABEL, "%s", msg);
            break;

        case ADB__LOG_COUNT:
        default:
            break;
    }
}

static void query_command(
        const aparse_arg *args,
        void *param)
{
    adb_error_t err = ADB_ERR_OK;
    adb_ctx_t *ctx = NULL;
    adb_wired_info_t **infos = NULL;
    size_t count = 0;

    (void)args;
    (void)param;
    
    CHECK(adb_ctx_create(&ctx), err, cleanup, "failed to create libadb context");
    CHECK(adb_query_wired(ctx, &infos, &count), 
            err, cleanup, "failed to query wired devices");
    for(size_t i = 0; i < count; i++)
    {
        adb_wired_info_t *conn_info = infos[i];
        info("Device %zu: %s - %s", i,
                adb_wired_info_manufacturer(conn_info),
                adb_wired_info_product(conn_info));
    }
cleanup:
    adb_ctx_destroy(ctx);
}

static const char *get_home_dir(void)
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

static bool parse_ip(
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

static bool is_file_exist(
        const char *path)
{
    FILE *file = fopen(path, "r");
    if(!file)
        return errno == ENOENT ? false : true;
    fclose(file);
    return true;
}

static void pair_command(
        const aparse_arg *args,
        void *param)
{
    const char *ip = ((const char**)param)[0];
    const char *code = ((const char**)param)[1];

    char key_path[PATH_MAX] = {0};
    char host[INET6_ADDRSTRLEN] = {0};
    uint16_t port = 0;

    adb_error_t err = ADB_ERR_OK;
    adb_ctx_t *ctx = NULL;
    adb_conn_t *conn = NULL;
    adb_key_t *key = NULL;

    (void)args;

    if(!parse_ip(ip, host, sizeof(host), &port))
    {
        error("failed to parse \"%s\" as host:port", ip);
        return;
    }
    
    CHECK(adb_ctx_create(&ctx), 
            err, cleanup, "failed to create libadb context");
    CHECK(adb_conn_create_wireless(&conn, ctx, host, port),
            err, cleanup, "failed to create connection for pairing");

    snprintf(key_path, sizeof(key_path), "%s/%s",
            get_home_dir(), ".android/adbkey");
    if(is_file_exist(key_path))
    {
        CHECK(adb_key_load(&key, ctx, key_path), 
                err, cleanup, "failed to load key from \"%s\"", key_path);
    } else {
        info("no key was found, generating a new one");
        CHECK(adb_key_generate(&key, ctx),
                err, cleanup, "failed to generate new key");
        CHECK(adb_key_save(key, key_path), 
                err, cleanup, "failed to save key to \"%s\"", key_path);
    }

    CHECK(adb_pair(conn, code, key, NULL, 0),
            err, cleanup, "failed to pair with given device");

cleanup:
    adb_key_destroy(key);
    adb_conn_destroy(conn);
    adb_ctx_destroy(ctx);
}

static bool display_qr(
        const char *payload)
{
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tmpbuf[qrcodegen_BUFFER_LEN_MAX];

    bool status = qrcodegen_encodeText(
        payload, tmpbuf, qrcode,
        qrcodegen_Ecc_HIGH,
        qrcodegen_VERSION_MIN,
        qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO,
        true
    );
    if(!status)
        return false;
    
    int size = qrcodegen_getSize(qrcode);
    int border = 4;

    for (int y = -border; y < size + border; y++) {
        for (int x = -border; x < size + border; x++) {

            bool isBlack = false;

            if (x >= 0 && x < size && y >= 0 && y < size) {
                isBlack = qrcodegen_getModule(qrcode, x, y);
            }

            if (isBlack)
                printf("  ");  // black
            else
                printf("\u2588\u2588");            // white
        }
        printf("\n");
    }
    return true;
}

static void pair_qr_command(
        const aparse_arg *args,
        void *param)
{
    char service_name[32] = {0};
    char secret[32] = {0};
    char payload[128] = {0};

    adb_wireless_info_t *info = NULL;
    char key_path[PATH_MAX] = {0};

    adb_error_t err = ADB_ERR_OK;
    adb_ctx_t *ctx = NULL;
    adb_conn_t *conn = NULL;
    adb_key_t *key = NULL;

    (void)args;
    (void)param;

    CHECK(adb_ctx_create(&ctx), 
            err, cleanup, "failed to create libadb context");
    CHECK(adb_pair_qr_build_payload(ctx, 
                service_name, sizeof(service_name),
                secret, sizeof(secret)),
            err, cleanup, "failed to build QR payload");
    CHECK(adb_pair_qr_encode_payload(service_name, secret, 
                payload, sizeof(payload)),
            err, cleanup, "failed to encode QR code");
    display_qr(payload);

    struct timespec start = {0}, now = {0};
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(;;)
    {
        double elapsed = 0.0;
        err = adb_find_wireless_pairing(ctx, 
                    service_name, &info);
        if(err == ADB_ERR_TIMEOUT)
            continue;
        else if(err != ADB_ERR_OK)
            goto cleanup;

        if(info)
            break;
        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = (double)(now.tv_sec - start.tv_sec) + 
            (double)(now.tv_nsec - start.tv_nsec) / 1e9;
        if(elapsed >= 30)
        {
            err = ADB_ERR_TIMEOUT;
            error("no device was found for %d secs", 30);
            goto cleanup;
        }
    }

    CHECK(adb_conn_create_wireless_from_info(&conn, ctx, info),
            err, cleanup, "failed to create connection for pairing");

    snprintf(key_path, sizeof(key_path), "%s/%s",
            get_home_dir(), ".android/adbkey");
    if(is_file_exist(key_path))
    {
        CHECK(adb_key_load(&key, ctx, key_path), 
                err, cleanup, "failed to load key from \"%s\"", key_path);
    } else {
        info("no key was found, generating a new one");
        CHECK(adb_key_generate(&key, ctx),
                err, cleanup, "failed to generate new key");
        CHECK(adb_key_save(key, key_path), 
                err, cleanup, "failed to save key to \"%s\"", key_path);
    }

    CHECK(adb_pair_qr(conn, secret, key, NULL, 0),
            err, cleanup, "failed to pair with given device");

cleanup:
    adb_key_destroy(key);
    adb_conn_destroy(conn);
    adb_ctx_destroy(ctx);
}

static int write_fn(
        void *userdata,
        const uint8_t *buf,
        size_t size)
{
    return (int)fwrite(buf, 1, size, userdata);
}

static void connect_command(
        const aparse_arg *args,
        void *param)
{
    const char *ip = ((const char**)param)[0];

    char key_path[PATH_MAX] = {0};
    char host[INET6_ADDRSTRLEN] = {0};
    uint16_t port = 0;

    adb_error_t err = ADB_ERR_OK;
    adb_ctx_t *ctx = NULL;
    adb_conn_t *conn = NULL;
    adb_key_t *key = NULL;

    (void)args;

    if(!parse_ip(ip, host, sizeof(host), &port))
    {
        error("failed to parse \"%s\" as host:port", ip);
        return;
    }

    CHECK(adb_ctx_create(&ctx), 
            err, cleanup, "failed to create libadb context");
    CHECK(adb_conn_create_wireless(&conn, ctx, host, port),
            err, cleanup, "failed to create connection for pairing");
    snprintf(key_path, sizeof(key_path), "%s/%s",
            get_home_dir(), ".android/adbkey");
    if(is_file_exist(key_path))
    {
        CHECK(adb_key_load(&key, ctx, key_path), 
                err, cleanup, "failed to load key from \"%s\"", key_path);
    } else {
        error("no key was found at \"%s\", "
                "please re-pair with the device for a new one",
                key_path);
        goto cleanup;
    }
    CHECK(adb_handshake(conn, key), 
            err, cleanup, "failed to perform handshake with %s", ip);
    FILE *file = fopen("./Shake Na Baby.webm", "wb");
    adb_pull(conn, "/storage/emulated/0/Download/Shake Na Baby.webm", 
            write_fn, file);
    fclose(file);

cleanup:
    adb_key_destroy(key);
    adb_conn_destroy(conn);
    adb_ctx_destroy(ctx);
}


static void pubkey_command(
        const aparse_arg *args, 
        void *param)
{
    const char *path = *(const char**)param;
    const char *output = ((const char**)param)[1];

    adb_error_t err = ADB_ERR_OK;
    adb_ctx_t *ctx = NULL;
    adb_key_t *key = NULL;
    uint8_t pubkey[2048] = {0};
    size_t pubkey_size = 0;

    (void)args;

    CHECK(adb_ctx_create(&ctx),
            err, cleanup, "failed to create libadb context");
    CHECK(adb_key_load(&key, ctx, path),
            err, cleanup, "failed to load adb private key");
    CHECK(adb_key_generate_pubkey(
                key, pubkey, sizeof(pubkey), &pubkey_size),
            err, cleanup, "failed to generate public key from private key");

    if(output)
    {
        FILE *file = NULL;
        size_t pubkey_len = 0;

        file = fopen(output, "w");
        if(!file)
        {
            error("failed to open \"%s\"", output);
            info("reason: %s", strerror(errno));
            goto cleanup;
        }

        pubkey_len = strlen((char*)pubkey);
        if(fwrite(pubkey, 1, pubkey_len, file) != pubkey_len)
        {
            error("failed to write key to \"%s\"", output);
            info("reason: %s", strerror(errno));
            goto cleanup;
        }

        fclose(file);
    } else
        printf("%s\n", pubkey);

cleanup:
    adb_key_destroy(key);
    adb_ctx_destroy(ctx);
}

int main(int argc, char **argv)
{
    aparse_arg pair_args[] =
    {
        aparse_arg_string(
                "ip", 
                NULL, 0, 
                "IP to target device (host:port)"),
        aparse_arg_string(
                "code",
                NULL, 0, 
                "Pairing code alongside with the IP"),
        aparse_arg_end_marker
    };
    
    aparse_arg connect_args[] =
    {
        aparse_arg_string(
                "ip", 
                NULL, 0, 
                "IP to target device (host:port)"),
        aparse_arg_end_marker
    };

    aparse_arg pubkey_args[] = 
    {
        aparse_arg_string("path", 
                NULL, 0, 
                "Path to RSA-2048 private key"),
        aparse_arg_option(
                "-o", "--output", 
                NULL, 0,
                APARSE_ARG_TYPE_STRING,
                "Path to output file"),
        aparse_arg_end_marker
    };
    aparse_arg commands[] =
    {
        aparse_arg_subparser_impl(
                "pair", 
                pair_args, pair_command, 
                NULL, 0, 
                "Pair wireless ADB device through TCP",
                (size_t[]){
                    0, sizeof(const char*),
                    sizeof(const char*), sizeof(const char*)
                }, 2),
        aparse_arg_subparser_impl(
                "pair-qr", 
                NULL, pair_qr_command, 
                NULL, 0, 
                "Pair wireless ADB device through TCP with QR", 
                NULL, 0),
        aparse_arg_subparser_impl(
                "connect", 
                connect_args, connect_command, 
                NULL, 0, 
                "Connect wireless ADB device through TCP",
                (size_t[]){
                    0, sizeof(const char*),
                }, 1),
        aparse_arg_subparser(
                "query",
                NULL, query_command,
                NULL, 0,
                "Query all USB connected ADB devices"),
        aparse_arg_subparser_impl(
                "pubkey", 
                pubkey_args, pubkey_command, 
                NULL, 0, 
                "Generate public key from private key", 
                (size_t[]){
                    0, sizeof(void*),
                    sizeof(void*), sizeof(void*)
                }, 2),
        aparse_arg_end_marker
    };
    aparse_arg main_args[] =
    {
        aparse_arg_parser("command", commands),
        aparse_arg_end_marker
    };
    aparse_list dispatch = {0};

    if(aparse_parse(
            argc, argv,
            main_args, &dispatch,
            "adb cli interface") != APARSE_STATUS_OK)
        return 1;

    adb_set_log_callback(log_callback, NULL, 
            ADB_LOG_DEBUG);
    aparse_dispatch_all(&dispatch);
    return 0;
}
