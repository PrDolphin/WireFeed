#ifndef CONTPCB_LIB_HPP
#define CONTPCB_LIB_HPP

#define ANALOG1 A6
#define ANALOG2 A7

#define ENC_BUT1   6
#define ENC_BUT2   7
#define ARR_BUT1   11
#define ARR_BUT2   12
#define ARR_BUT3   A0
#define ARR_BUT4   A1
#define ARR_BUT5   A2
#define ARR_BUT6   A3

#define PWM_PIN1 9
#define PWM_PIN2 10

#define PWM_MODE_COUNTER 0
// CTC with ICR as TOP
#define PWM_MODE_CTC (_BV(WGM13) | _BV(WGM12))
// Fast PWM with ICR as TOP
#define PWM_MODE_FAST (_BV(WGM13) | _BV(WGM12) | _BV(WGM11))
// Phase correct PWM with ICR as TOP
#define PWM_MODE_PC (_BV(WGM13) | _BV(WGM11))
// Phase and frequency correct PWM with ICR as TOP
#define PWM_MODE_PFC _BV(WGM13)

#define PWM_PRE_1 _BV(CS10)
#define PWM_PRE_8 _BV(CS11)
#define PWM_PRE_64 (_BV(CS11) | _BV(CS10))
#define PWM_PRE_256 _BV(CS12)
#define PWM_PRE_1024 (_BV(CS12) | _BV(CS10))
#define PWM_PRE_EXT_FALL (_BV(CS12) | _BV(CS11))
#define PWM_PRE_EXT_RISE (_BV(CS12) | _BV(CS11) | _BV(CS10))

#ifndef AVOID_I2C_PINS
#define SOFTI2C_DIO SDA
#define SOFTI2C_CLK 8
#define SOFTI2C_CLK2 13
#else
#define SOFTI2C_DIO 13
#define SOFTI2C_CLK 8
#endif

#define ENCODER1_CONNECTED 0x1
#define ENCODER2_CONNECTED 0x2

#ifdef __cplusplus
extern "C" {
#endif

extern volatile int16_t encoder_pos[2];
extern uint8_t encoder_step_multiplier[2];

uint16_t analog_read(uint8_t pin);
void pwm_setup(uint8_t prescaler, uint16_t mode, uint16_t top = -1);
void analog_write(uint8_t pin, uint16_t duty);
void analog_write_stop(uint8_t pin);
void pcb_init(uint8_t flags);

#ifdef __cplusplus
}
#endif

#endif // CONTPCB_LIB_HPP