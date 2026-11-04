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

        aparse_arg disconnect_args[] =
        {
                aparse_arg_string(
                                "serial",
                                NULL, 0,
                                "Serial or endpoint of the connected device"),
                aparse_arg_end_marker
        };
    
    aparse_arg push_args[] =
    {
        aparse_arg_array(
                "local_path", 
                NULL,
                sizeof(aparse_list), sizeof(char *),
                APARSE_ARG_TYPE_STRING, 0,
                "Source file on the host (repeatable)"),
        aparse_arg_string(
                "remote_path", 
                NULL, 0, 
                "Destination path on the device (directory for multiple sources)"),
        aparse_arg_end_marker
    };

        aparse_arg pull_args[] =
        {
                aparse_arg_array(
                                "remote_path",
                                NULL, sizeof(aparse_list), sizeof(char *),
                                APARSE_ARG_TYPE_STRING, 0,
                                "Path to source file on the device"),
                aparse_arg_string(
                                "local_path",
                                NULL, 0,
                                "Destination file or directory on the host"),
                aparse_arg_end_marker
        };

        aparse_arg stat_args[] =
        {
                aparse_arg_string(
                                "path",
                                NULL, 0,
                                "Path to the remote file"),
                aparse_arg_end_marker
        };

        aparse_arg ls_args[] =
        {
                aparse_arg_string(
                                "path",
                                NULL, 0,
                                "Path to the remote directory"),
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
        aparse_arg_subparser("disconnect", disconnect_args, disconnect_cmd,
                &options, sizeof(options),
                "Disconnect a connected device",
                adbc_cmd_options_t, serial),
        aparse_arg_subparser("push", push_args, push_cmd, 
                &options, sizeof(options), 
                "Copy local file(s) to device",
                adbc_cmd_options_t, push_local_paths, remote_path),
        aparse_arg_subparser("pull", pull_args, pull_cmd,
                &options, sizeof(options),
                "Copy files from device to local host",
                adbc_cmd_options_t, pull_remote_paths, pull_local_path),
        aparse_arg_subparser("stat", stat_args, stat_cmd,
                &options, sizeof(options),
                "Show remote file information",
                adbc_cmd_options_t, stat_path),
        aparse_arg_subparser("ls", ls_args, ls_cmd,
                &options, sizeof(options),
                "List remote directory entries",
                adbc_cmd_options_t, ls_path),
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
        {
                aparse_list_free(&options.push_local_paths);
                aparse_list_free(&options.pull_remote_paths);
        return 1;
        }

    if(level_str != NULL)
    {
        if(!adbc_log_level_parse(level_str, &options.opt_lvl))
        {
            ADBC_ERROR("unknown log level was specified");
            ADBC_INFO("supported: \"debug\", \"info\", \"warn\", \"error\", "
                    "got: \"%s\"", level_str);
            aparse_list_free(&options.push_local_paths);
            aparse_list_free(&options.pull_remote_paths);
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
        aparse_list_free(&options.push_local_paths);
        aparse_list_free(&options.pull_remote_paths);
    return 0;
}
