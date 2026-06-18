#pragma once


#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 1
#define FW_MAGIC 0xDEADBEEF  // sanity check
#define BANK1_FW_INFO_ADDR  ((fw_info_t *)0x0800024C/*0x080001F8*/)
#define BANK2_FW_INFO_ADDR  ((fw_info_t *)0x0810024C/*0x081001F8*/)


typedef struct __attribute__((aligned(4))) {
    uint32_t _pad0[2];      // 8 bytes
    uint32_t magic;         // 4 bytes
    uint8_t major;          // 1 byte
    uint8_t minor;          // 1 byte
    uint8_t patch;          // 1 byte
    uint8_t _pad1;          // 1 byte padding -> aligns next field to 4 bytes
    char build_time[30];    // next 30 bytes
    uint8_t _pad2[2];       // pad to make struct size multiple of 4
} fw_info_t;


typedef struct {
    uint8_t running_bank;            // 1 or 2
    uint8_t standby_bank;            // 1 or 2
    const fw_info_t *running_fw;      // pointer to FW info
    const fw_info_t *standby_fw;      // pointer to FW info
} fw_bank_status_t;
