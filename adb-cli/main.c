#include <aparse.h>

int main(int argc, char **argv)
{
    aparse_arg main_args[] =
    {
        aparse_arg_end_marker
    };

    if(aparse_parse(
            argc, argv,
            main_args, NULL,
            "adb cli interface") != APARSE_STATUS_OK)
        return 1;
    return 0;
}