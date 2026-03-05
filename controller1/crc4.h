#ifndef CRC4_H
#define CRC4_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint8_t crc4(uint8_t *message, uint8_t message_size);

#ifdef __cplusplus
}
#endif

#endif // CRC4_H