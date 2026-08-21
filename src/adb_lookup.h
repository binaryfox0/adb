#ifndef ADB_LOOKUP_H
#define ADB_LOOKUP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

bool adb__lookup_usb(
        const uint16_t vid,
        const uint16_t pid,
        char *manufacturer,
        const size_t manufacturer_size,
        char *product,
        const size_t product_size);

#endif