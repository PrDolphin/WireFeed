#ifndef CONTPCB_LIB_HPP
#define CONTPCB_LIB_HPP

#define ANALOG1 0
#define ANALOG2 1

#define ENC_BUT1   0
#define ENC_BUT2   1
#define ARR_BUT1   2
#define ARR_BUT2   7
#define ARR_BUT3   3
#define ARR_BUT4   8
#define ARR_BUT5   4
#define ARR_BUT6   9
#define ARR_BUT7   5
#define ARR_BUT8  10
#define ARR_BUT9   6
#define ARR_BUT10 11

#define PWM_PIN1 0
#define PWM_PIN2 1

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

#define TWIRE_DIO 8
#define TWIRE_CLK1 6
#define TWIRE_CLK2 7
#define TWIRE_CLK3 13

#ifdef __cplusplus
extern "C" {
#endif

extern uint16_t buttons;
extern volatile int16_t encoder_pos[2];

uint16_t analog_read(uint8_t pin);
bool digital_read(uint8_t button);
void pwm_setup(uint8_t prescaler, uint16_t mode, uint16_t top = -1);
void analog_write(uint8_t pin, uint16_t duty);
void analog_write_stop(uint8_t pin);
void pcb_init();
void pcb_read();

#ifdef __cplusplus
}
#endif

#endif // CONTPCB_LIB_HPP