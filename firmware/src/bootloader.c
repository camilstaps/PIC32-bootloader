#include "bootloader.h"

const uint16_t blt_crc_table[] = {0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 
    0x60c6, 0x70e7, 0x8108, 0x9129, 0xa144, 0xb16b, 0xc18c, 0xd1ad, 0xe1c1, 0xf1ef};

void blt_crc16(uint16_t* crc, uint8_t data) {
    uint16_t i;
    
    i = (*crc >> 12) ^ (data >> 4);
    *crc = blt_crc_table[i & 0x0f] ^ (*crc << 4);
    i = (*crc >> 12) ^ (data >> 0);
    *crc = blt_crc_table[i & 0x0f] ^ (*crc << 4);
}

void blt_nvm_operation(uint32_t nvmop) {
    // This is code from harmony bootloader/src/nvm.c
    // Disable flash write/erase operations
    PLIB_NVM_MemoryModifyInhibit(NVM_ID_0);
    PLIB_NVM_MemoryOperationSelect(NVM_ID_0, nvmop);
    // Allow memory modifications
    PLIB_NVM_MemoryModifyEnable(NVM_ID_0);
    // Unlock flash
    PLIB_NVM_FlashWriteKeySequence(NVM_ID_0, 0);
    PLIB_NVM_FlashWriteKeySequence(NVM_ID_0, DRV_NVM_PROGRAM_UNLOCK_KEY1);
    PLIB_NVM_FlashWriteKeySequence(NVM_ID_0, DRV_NVM_PROGRAM_UNLOCK_KEY2);
    // Do operation
    PLIB_NVM_FlashWriteStart(NVM_ID_0);
}

void blt_nvm_write_word(void* address, uint32_t data) {
    uint32_t addr = KVA_TO_PA((uint32_t) address);
    PLIB_NVM_FlashAddressToModify(NVM_ID_0, addr);
    PLIB_NVM_FlashProvideData(NVM_ID_0, data);
    blt_nvm_operation(WORD_PROGRAM_OPERATION);
}

void blt_nvm_clear_error(void) {
    blt_nvm_operation(NO_OPERATION);
}

void blt_wait(blt_handle *handle) {
    handle->state = BLT_STATE_WAIT_FOR_COMMAND;
    handle->crc = 0;
    uint8_t i;
    for (i=0; i < 5; i++)
        handle->cmd[i] = 0x00;
    handle->cmd_ptr = 0;
}

void blt_initialise(blt_handle *handle) {
    TRISBbits.TRISB1 = 0;
    
    handle->cmd_ptr = 0;
    handle->crc = 0;
    handle->hex_base_addr = 0;
    handle->hex_ptr = 0;
    handle->resp_len = 0;
    handle->resp_ptr = 0;
    
    blt_wait(handle);
}

void blt_send_response(blt_handle *handle, uint8_t* data, uint8_t len) {
    uint8_t i;
    for (i=0; i < 10; i++)
        handle->resp[i] = 0x00;
    handle->resp_len = 0;
    
    handle->resp[handle->resp_len++] = 0x01;
    
    uint16_t crc = 0;
    for (i=0; i<len; i++) {
        if (*data == 0x01 || *data == 0x04 || *data == 0x10)
            handle->resp[handle->resp_len++] = 0x10;
        handle->resp[handle->resp_len++] = *data;
        blt_crc16(&crc, *data);
        data++;
    }
    
    handle->resp[handle->resp_len++] = crc >> 8;
    handle->resp[handle->resp_len++] = crc & 0x00ff;
    handle->resp[handle->resp_len++] = 0x04;
    
    handle->resp_ptr = 0;
    handle->state = BLT_STATE_SENDING_RESPONSE;
}

void blt_handle_command(blt_handle *handle) {
    uint8_t resp[4];
    void (*app_addr)(void);
    
    resp[0] = resp[1] = resp[2] = resp[3] = 0x00;
    switch (handle->cmd[1]) {
        case 0x01: // version
            resp[0] = 0x01;
            resp[1] = BLT_VERSION >> 8;
            resp[2] = BLT_VERSION & 0x00ff;
            blt_send_response(handle, resp, 3);
            break;
        case 0x02: // erase
            handle->nvm_handle = DRV_NVM_Open(0, DRV_IO_INTENT_READWRITE);
            DRV_NVM_Erase(handle->nvm_handle, &handle->nvm_cmd_handle, 0, 512);
            if (handle->nvm_cmd_handle == DRV_NVM_COMMAND_HANDLE_INVALID) {
                blt_send_response(handle, "\xff", 1);
            } else {
                handle->state = BLT_STATE_ERASING;
            }
            break;
        case 0x03: // program flash
            blt_send_response(handle, "\x03", 1);
            break;
        case 0x04: // read CRC
            // @todo
            break;
        case 0x05: // jump to application
            app_addr = (void(*)(void)) BLT_RESET_ADDR;
            app_addr();
            while (1);
            break;
        default: // unknown command
            // @todo
            break;
    }
}

void blt_handle_hex_record(blt_handle* handle) {
    hex_record rec;
    
    handle->hex_ptr = 0;
    
    hex_read_record_ascii(&rec, handle->hex);
    if (!hex_verify(&rec))
        return;
    
    void* addr;
    uint32_t data;
    uint8_t data_ptr;
    switch (rec.type) {
        case HEX_TYPE_DATA:
            addr = PA_TO_KVA0((uint32_t) (handle->hex_base_addr + ((uint32_t) rec.addr)));
            data_ptr = 0;
            while (rec.len > 0) {
                if ((void*) BLT_FLASH_BASE <= addr && addr <= (void*) BLT_FLASH_END) {
                    data = 0xffffffff;
                    if (rec.len < 4) {
                        memcpy(&data, &rec.data[data_ptr], rec.len);
                    } else {
                        memcpy(&data, &rec.data[data_ptr], 4);
                    }
                    
                    blt_nvm_write_word(addr, data);
                    
                    while (!PLIB_NVM_FlashWriteCycleHasCompleted(NVM_ID_0));
                    if (PLIB_NVM_WriteOperationHasTerminated(NVM_ID_0)) {
                        blt_nvm_clear_error();
                    }
                }
                    
                addr += 4;
                data_ptr += 4;
                if (rec.len < 4)
                    rec.len = 0;
                else
                    rec.len -= 4;
            }
            handle->state = BLT_STATE_WAIT_FOR_HEX_RECORD;
            break;
        case HEX_TYPE_ELA:
            handle->hex_base_addr = (rec.data[0] << 24) + (rec.data[1] << 16);
            handle->state = BLT_STATE_WAIT_FOR_HEX_RECORD;
            break;
        case HEX_TYPE_EOF:
            handle->state = BLT_STATE_READING_COMMAND;
            break;
    }
}

void blt_tasks(blt_handle *handle) {
    DRV_NVM_COMMAND_STATUS status;
    
    switch (handle->state) {
        case BLT_STATE_SENDING_RESPONSE:
            if (handle->resp_ptr > handle->resp_len) {
                blt_wait(handle);
            } else {
                handle->writeByte(handle->resp[handle->resp_ptr++]);
            }
            break;
        case BLT_STATE_ERASING:
            status = DRV_NVM_CommandStatus(handle->nvm_handle, handle->nvm_cmd_handle);
            if (status == DRV_NVM_COMMAND_ERROR_UNKNOWN) {
                blt_send_response(handle, "\xff", 1);
            } else if (status == DRV_NVM_COMMAND_COMPLETED) {
                blt_send_response(handle, "\x02", 1);
            }
            break;
        default:
            while (!handle->noByteReady()) {
                uint8_t byte = handle->readByte();

                blt_crc16(&handle->crc, byte);

                switch (handle->state) {
                    case BLT_STATE_WAIT_FOR_COMMAND:
                        if (byte == 0x01) {
                            handle->state = BLT_STATE_READING_COMMAND;
                            handle->cmd[handle->cmd_ptr++] = byte;
                        }
                        break;
                    case BLT_STATE_READING_COMMAND:
                        if (byte == 0x03 && handle->cmd_ptr == 1) {
                            handle->cmd[handle->cmd_ptr++] = byte;
                            handle->hex_base_addr = 0;
                            handle->hex_ptr = 0;
                            handle->state = BLT_STATE_WAIT_FOR_HEX_RECORD;
                        } else if (byte == 0x10) {
                            handle->state = BLT_STATE_READING_COMMAND_ESC;
                        } else if (byte == 0x04) {
                            handle->cmd[handle->cmd_ptr++] = byte;
                            blt_handle_command(handle);
                        } else {
                            handle->cmd[handle->cmd_ptr++] = byte;
                        }
                        break;
                    case BLT_STATE_READING_COMMAND_ESC:
                        handle->cmd[handle->cmd_ptr++] = byte;
                        handle->state = BLT_STATE_READING_COMMAND;
                        break;
                    case BLT_STATE_WAIT_FOR_HEX_RECORD:
                        handle->hex_ptr = 0;
                        if (byte == ':') {
                            handle->state = BLT_STATE_READING_HEX_RECORD;
                        } else if (byte == 0x10) {
                            handle->state = BLT_STATE_WAIT_FOR_HEX_RECORD_ESC;
                        }
                        break;
                    case BLT_STATE_WAIT_FOR_HEX_RECORD_ESC:
                        if (byte == ':') {
                            handle->state = BLT_STATE_READING_HEX_RECORD;
                        } else {
                            handle->state = BLT_STATE_WAIT_FOR_HEX_RECORD;
                        }
                        break;
                    case BLT_STATE_READING_HEX_RECORD:
                        if (byte == 0x10) {
                            handle->state = BLT_STATE_READING_HEX_RECORD_ESC;
                        } else if (byte == '\n') {
                            blt_handle_hex_record(handle);
                        } else {
                            handle->hex[handle->hex_ptr++] = byte;
                        }
                        break;
                    case BLT_STATE_READING_HEX_RECORD_ESC:
                        handle->hex[handle->hex_ptr++] = byte;
                        handle->state = BLT_STATE_READING_HEX_RECORD;
                        break;
                }
            }
            break;
    }
}
