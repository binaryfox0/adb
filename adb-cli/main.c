#include <aparse.h>

#include "adbc_log.h"
#include "adbc_client.h"
#include "adbc_server.h"

int main(int argc, char **argv)
{
    const char *level_str = NULL;
    adbc_cmd_options_t options = {0};
    aparse_list dispatch = {0};
    aparse_arg commands[] =
    {
        aparse_arg_subparser(
                "start-server", NULL, start_server_cmd, &options,
                sizeof(options), "Ensure that there is a server running"),
        aparse_arg_subparser(
                "kill-server", NULL, kill_server_cmd, &options,
                sizeof(options), "Kill the server if it is running"),
        aparse_arg_subparser(
                "devices", NULL, devices_cmd, &options,
                sizeof(options), "List connected devices"),
        aparse_arg_subparser("pair-qr", NULL, pair_qr_cmd, 
                &options, sizeof(options), 
                "pair with a device for secure TCP/IP communication using QR code"),
        aparse_arg_end_marker
    };
    aparse_arg main_args[] =
    {
        aparse_arg_parser("command", commands),
        aparse_arg_option(
                "-lvl", "--log-level", &level_str, 0,
                APARSE_ARG_TYPE_STRING, "Set libadb and adb-cli log level"),
        aparse_arg_end_marker
    };

    if(aparse_parse(argc, argv, main_args, &dispatch,
                "libadb command line utils") != APARSE_STATUS_OK)
        return 1;

    if(level_str != NULL)
    {
        if(!adbc_log_level_parse(level_str, &options.level))
        {
            ADBC_ERROR("unknown log level was specified");
            ADBC_INFO("supported: \"debug\", \"info\", \"warn\", \"error\", "
                    "got: \"%s\"", level_str);
            return 1;
        }
    }
    else
        options.level = ADB_LOG_ERROR;

    aparse_dispatch_all(&dispatch);
    return 0;
}
