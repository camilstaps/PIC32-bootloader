#include <stdbool.h>
#include <stdint.h>

#define HEX_DATA_LEN 16

#define HEX_TYPE_DATA 0
#define HEX_TYPE_EOF 1
#define HEX_TYPE_ELA 4

typedef struct {
    uint8_t len;
    uint16_t addr;
    uint8_t type;
    uint8_t data[HEX_DATA_LEN];
    uint8_t checksum;
} hex_record;

void hex_read_record_ascii(hex_record* to, uint8_t* from);
bool hex_verify(hex_record* rec);
