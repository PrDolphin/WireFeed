#include <avr/pgmspace.h>
#include <stdint.h>

const PROGMEM uint8_t crc4_table[16] = {
  0x0, 0x2, 0x4, 0x6, 0x8, 0xA, 0xC, 0xE,
  0x7, 0x5, 0x3, 0x1, 0xF, 0xD, 0xB, 0x9
};

uint8_t crc4(uint8_t *message, uint8_t message_size) {
  uint8_t rem = 0;
  for (uint8_t i = 0; i < message_size; ++i) {
    uint8_t msg = message[i];
    rem = pgm_read_byte(crc4_table + ((msg >> 4) ^ rem));
    rem = pgm_read_byte(crc4_table + ((msg & 0x0F) ^ rem));
  }
  return rem;
}