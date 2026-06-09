#ifndef ACCELERATION_H
#define ACCELERATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void processAcceleration();
void setSpeed(uint16_t speed);
void setCoefOff2(int16_t coef_off);

#ifdef __cplusplus
}
#endif

#endif // ACCELERATION_H
