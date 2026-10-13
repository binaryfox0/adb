#include <stdio.h>

#include <aparse.h>

#include "adbc_utils.h"
#include "adbc_log.h"
#include "adbc_client.h"
#include "adbc_server.h"

int main(int argc, char **argv)
{
    const char *level_str = NULL;
    adbc_cmd_options_t options = {0};
    aparse_list dispatch = {0};
    char path[4096] = {0};

    aparse_arg pair_args[] =
    {
        aparse_arg_string(
                "endpoint", 
                NULL, 0, 
                "Endpoint of the target device (host:port)"),
        aparse_arg_string(
                "code",
                NULL, 0, 
                "Pairing code for pairing"),
        aparse_arg_end_marker
    };
    
    aparse_arg connect_args[] =
    {
        aparse_arg_string(
                "endpoint", 
                NULL, 0, 
                "Endpoint of the target device (host:port)"),
        aparse_arg_end_marker
    };

    aparse_arg commands[] =
    {
        aparse_arg_subparser(
                "start-server", NULL, start_server_cmd, 
                &options, sizeof(options), 
                "Ensure that there is a server running"),
        aparse_arg_subparser(
                "kill-server", NULL, kill_server_cmd, &options,
                sizeof(options), 
                "Kill the server if it is running"),
        aparse_arg_subparser(
                "devices", NULL, devices_cmd, 
                &options, sizeof(options), 
                "List connected devices"),
        aparse_arg_subparser("pair", pair_args, pair_cmd, 
                &options, sizeof(options), 
                "Pair with a device for secure TCP/IP communication",
                adbc_cmd_options_t, endpoint, code),
        aparse_arg_subparser("pair-qr", NULL, pair_qr_cmd, 
                &options, sizeof(options), 
                "Pair with a device for secure TCP/IP communication using QR code"),
        aparse_arg_subparser("connect", connect_args, connect_cmd, 
                &options, sizeof(options), 
                "Connect to a device via TCP/IP", 
                adbc_cmd_options_t, endpoint),
        aparse_arg_end_marker
    };
    aparse_arg main_args[] =
    {
        aparse_arg_parser("command", commands),
        aparse_arg_option(
                "-lvl", "--log-level", 
                &level_str, 0,
                APARSE_ARG_TYPE_STRING, 
                "Set libadb and adb-cli log level"),
        aparse_arg_option(
                "-key", "--key-file", 
                &options.opt_key, 0,
                APARSE_ARG_TYPE_STRING, 
                "Path to RSA-2048 PEM file"),
        aparse_arg_end_marker
    };

    if(aparse_parse(argc, argv, main_args, &dispatch,
                "libadb command line utils") != APARSE_STATUS_OK)
        return 1;

    if(level_str != NULL)
    {
        if(!adbc_log_level_parse(level_str, &options.opt_lvl))
        {
            ADBC_ERROR("unknown log level was specified");
            ADBC_INFO("supported: \"debug\", \"info\", \"warn\", \"error\", "
                    "got: \"%s\"", level_str);
            return 1;
        }
    }
    else
        options.opt_lvl = ADB_LOG_ERROR;

    if(!options.opt_key)
    {
        snprintf(path, sizeof(path), "%s/%s", 
                adbc_util_home_dir(), ".android/adbkey");
        options.opt_key = path;
    }

    aparse_dispatch_all(&dispatch);
    return 0;
}
