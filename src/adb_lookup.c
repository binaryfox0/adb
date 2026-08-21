#include "adb_lookup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#define ADB__ARRSZ(x) (sizeof((x)) / sizeof(((x))[0]))

static bool adb__lookup_usb_impl(
        const char *path,
        const uint16_t vid,
        const uint16_t pid,
        char *manufacturer,
        const size_t manufacturer_size,
        char *product,
        const size_t product_size)
{
    FILE *file = NULL;
    char line[4096] = {0};
    unsigned int current_vid = 0;
    
    file = fopen(path, "r");
    if (!file)
        return false;

    manufacturer[0] = '\0';
    product[0] = '\0';
    while (fgets(line, sizeof(line), file)) 
    {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n')
            continue;

        /* Vendor line */
        if (line[0] != '\t' && line[0] != ' ') {
            unsigned int id;
            char name[4096];

            if (sscanf(line, "%x %4095[^\n]", &id, name) == 2) {
                current_vid = id;

                if (id == vid)
                    snprintf(manufacturer, manufacturer_size, "%s", name);
            }

            continue;
        }

        /* Product line */
        if (current_vid == vid) 
        {
            unsigned int id = 0;
            char name[4096] = {0};

            if (sscanf(line, "%x %4095[^\n]", &id, name) == 2) 
            {
                if (id == pid) 
                {
                    snprintf(product, product_size, "%s", name);
                    fclose(file);
                    return true;
                }
            }
        }
    }

    fclose(file);
    return false;
}

bool adb__lookup_usb(
        const uint16_t vid,
        const uint16_t pid,
        char *vendor,
        const size_t vendor_size,
        char *product,
        const size_t product_size)
{
    static const char *paths[] = 
    {
        "/usr/share/hwdata/usb.ids",
        "/usr/share/usb.ids",
        "/var/lib/usbutils/usb.ids",
        "/usr/share/misc/usb.ids",
        "/usr/local/share/usb.ids",
        NULL
    };

    const char *env = getenv("ADB_USB_IDS");
    if(env)
    {
        return adb__lookup_usb_impl(env, 
                vid, pid, 
                vendor, vendor_size, 
                product, product_size);
    } else {
        for(size_t i = 0; i < ADB__ARRSZ(paths); i++)
        {
            if(access(paths[i], R_OK) != 0)
                continue;

            if(adb__lookup_usb_impl(paths[i], 
                    vid, pid, 
                    vendor, vendor_size, 
                    product, product_size))
                return true;
        }
    }

    return false;
}