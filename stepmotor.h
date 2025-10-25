#ifndef STEPMOTOR_H
#define STEPMOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t motor_constants[2];
extern uint16_t motor_intervals[2];
extern uint16_t target_speed;
void motors_init();
void motors_update_state();
uint16_t accelerate (uint8_t n_steps);
#define motors_enable_all() \
TCCR1A |= (1 << COM1A0) | (1 << COM1B0), \
OCR1A = motor_intervals[0] + TCNT1, \
OCR1B = motor_intervals[1] + TCNT1
#define motors_disable_all() \
TCCR1A &= ~((1 << COM1A0) | (1 << COM1B0))

#ifdef __cplusplus
}
#endif

#endif // STEPMOTOR_H