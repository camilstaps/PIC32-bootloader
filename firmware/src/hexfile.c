#include "hexfile.h"

uint8_t hex_ascii_to_hex_digit(uint8_t digit) {
    if ('0' <= digit && digit <= '9')
        return digit - '0';
    else if ('a' <= digit && digit <= 'f')
        return digit - 'a' + 10;
    else if ('A' <= digit && digit <= 'F')
        return digit - 'A' + 10;
    else
        return -1;
}

uint8_t hex_ascii_to_hex(uint8_t* from) {
    return 16 * hex_ascii_to_hex_digit(from[0]) + hex_ascii_to_hex_digit(from[1]);
}

void hex_read_record_ascii(hex_record* to, uint8_t* from) {
    to->len = hex_ascii_to_hex(from);
    to->addr = (hex_ascii_to_hex(from + 2) << 8) + hex_ascii_to_hex(from + 4);
    to->type = hex_ascii_to_hex(from + 6);
    uint8_t i;
    for (i = 0; i < HEX_DATA_LEN; i++)
        to->data[i] = 0x00;
    for (i = 0; i < to->len; i++)
        to->data[i] = hex_ascii_to_hex(from + 2*i + 8);
    to->checksum = hex_ascii_to_hex(from + 2*i + 8);
}

bool hex_verify(hex_record* rec) {
    uint8_t sum = 0;
    sum += rec->len;
    sum += (rec->addr >> 8) + (rec->addr & 0x00ff);
    sum += rec->type;
    uint8_t i;
    for (i = 0; i < rec->len; i++)
        sum += rec->data[i];
    sum += rec->checksum;
    return sum == 0;
}
