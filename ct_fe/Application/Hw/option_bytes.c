#include <stdio.h>
#include <stdint.h>
#include "tx_api.h"
#include "software_ver.h"


// Place this in a specific flash section
__attribute__((section(".fw_info"))) const fw_info_t fw_info = {
    .magic = FW_MAGIC,
    .major = FW_VERSION_MAJOR,
    .minor = FW_VERSION_MINOR,
    .patch = FW_VERSION_PATCH,
    .build_time = __DATE__ " " __TIME__ "\0"  // explicitly add null terminator
};
