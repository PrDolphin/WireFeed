#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

extern uint16_t motors_target_speed;
extern int16_t  speed_offset;
extern bool     motor_enabled;
extern uint8_t  managed_mode;

void commInit(void);

#ifdef __cplusplus
}
#endif

#endif // COMMUNICATION_H
