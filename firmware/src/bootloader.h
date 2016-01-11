#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sys/kmem.h>

#include "driver/nvm/drv_nvm.h"

#include "hexfile.h"

#define BLT_VERSION 0x0001

#define BLT_FLASH_BASE 0x9D000000
#define BLT_FLASH_END  0x9D07ffff
#define BLT_RESET_ADDR 0x9D00f000

typedef enum {
    BLT_STATE_UNINIT = 0,
    BLT_STATE_WAIT_FOR_COMMAND,
    BLT_STATE_READING_COMMAND,
    BLT_STATE_READING_COMMAND_ESC,
    BLT_STATE_WAIT_FOR_HEX_RECORD,
    BLT_STATE_WAIT_FOR_HEX_RECORD_ESC,
    BLT_STATE_READING_HEX_RECORD,
    BLT_STATE_READING_HEX_RECORD_ESC,
    BLT_STATE_ERASING,
    BLT_STATE_WRITING,
    BLT_STATE_SENDING_RESPONSE
} blt_state;

typedef struct {
    blt_state state;
    
    uint8_t cmd[5];
    uint8_t cmd_ptr;
    uint16_t crc;
    
    uint8_t hex[2 * (6 + HEX_DATA_LEN)];
    uint8_t hex_ptr;
    uint32_t hex_base_addr;
    
    uint8_t resp[10];
    uint8_t resp_ptr;
    uint8_t resp_len;
    
    DRV_HANDLE nvm_handle;
    DRV_NVM_COMMAND_HANDLE nvm_cmd_handle;
    
    void (*writeByte)(uint8_t);
    uint8_t (*readByte)(void);
    bool (*noByteReady)(void);
} blt_handle;

void blt_initialise(blt_handle*);
void blt_tasks(blt_handle*);
