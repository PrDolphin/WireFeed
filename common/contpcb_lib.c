#include <Arduino.h>
#include <stdint.h>

#define ENCODER1_DIR_PIN 5
#define ENCODER2_DIR_PIN 4
#define ENCODER1_BUTTON_PIN 1
#define ENCODER2_BUTTON_PIN 0
// Pins A0, A1, A2, A3
#define BUTTON_ARRAY_PC_MASK ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3))
#define BUTTON_ARRAY_PB_MASK (1 << 4)
#define BUTTON_ARRAY_READ_CONTROL_PIN 3
#define ANALOG_SAMPLE_CONST 3 /* 1/8 */
#define PWM1 1
#define PWM2 2

uint16_t buttons;
uint16_t analog_data[2];
volatile int16_t encoder_pos[2];

ISR(INT0_vect) {
  encoder_pos[0] += ((PIND >> (ENCODER1_DIR_PIN - 1)) & 2) - 1;
}

ISR(INT1_vect) {
  encoder_pos[1] += ((PIND >> (ENCODER2_DIR_PIN - 1)) & 2) - 1;
}

uint16_t analog_read(uint8_t pin) {
  pin -= (pin >= A6) ? A6 : 0;
  return (analog_data[pin] + (1 << (6 - 1))) >> 6; // Correct rounding
}

bool digital_read(uint8_t button) {
  return (buttons >> button) & 1;
}

void analog_write(uint8_t pin, uint16_t duty) {
  /* If timer runs in Normal or CTC mode, toggle pin instead of setting/clearing it */
  uint8_t is_normal_mode = (TCCR1A & (_BV(WGM11) | _BV(WGM10))) | (TCCR1B & (_BV(WGM13) | _BV(WGM12)));
  is_normal_mode = ((is_normal_mode & ~_BV(WGM12)) == 0) || (is_normal_mode == (_BV(WGM13) | _BV(WGM12)));
  switch (pin) {
    case 0:
      TCCR1A |= _BV(COM1A1) >> is_normal_mode;
      OCR1A = duty;
      break;
    case 1:
      TCCR1A |= _BV(COM1B1) >> is_normal_mode;
      OCR1B = duty;
      break;
  }
}

void analog_write_stop(uint8_t pin) {
  TCCR1A &= ~((_BV(COM1A1) | _BV(COM1A0)) >> (pin * 2));
}

void pwm_setup(uint8_t prescaler, uint16_t mode, uint16_t top) {
  TCCR1A = (_BV(WGM11) | _BV(WGM10)) & mode;
  TCCR1B = (_BV(WGM13) | _BV(WGM12)) & mode | prescaler;
  switch (mode) {
    case _BV(WGM12):
    case (_BV(WGM13) | _BV(WGM10)):
    case ((_BV(WGM13) | _BV(WGM12) | _BV(WGM11) | _BV(WGM10))):
      OCR1A = top;
      break;
    case _BV(WGM13):
    case (_BV(WGM13) | _BV(WGM11)):
    case (_BV(WGM13) | _BV(WGM12)):
    case (_BV(WGM13) | _BV(WGM12) | _BV(WGM11)):
      ICR1 = top;
      break;
  }
}

void pcb_read() {
  analog_data[0] = ((uint16_t)analogRead(A6) << (6-ANALOG_SAMPLE_CONST)) + (analog_data[0] - (analog_data[0] >> ANALOG_SAMPLE_CONST));
  analog_data[1] = ((uint16_t)analogRead(A7) << (6-ANALOG_SAMPLE_CONST)) + (analog_data[1] - (analog_data[1] >> ANALOG_SAMPLE_CONST));
  uint16_t inputs = PIND & ((1 << ENCODER1_BUTTON_PIN) | (1 << ENCODER2_BUTTON_PIN));
  inputs |= (PINB >> (4-2) & 0x4) | (PINC & 0xF) << 3;
  PORTB |= _BV(BUTTON_ARRAY_READ_CONTROL_PIN);
  inputs |= (PINB << (7-4) & 0x80) | (uint16_t)(PINC & 0xF) << 8;
  PORTB &= ~_BV(BUTTON_ARRAY_READ_CONTROL_PIN);
  buttons = inputs;
}

void pcb_init() {
  uint8_t sreg = SREG;
  cli();
  memset(encoder_pos, 0, sizeof(encoder_pos));
  SREG = sreg;
  memset(analog_data, 0 ,sizeof(analog_data));
  DDRB |= _BV(PWM1) | _BV(PWM2) | _BV(BUTTON_ARRAY_READ_CONTROL_PIN);
  PORTD |= _BV(ENCODER1_BUTTON_PIN) | _BV(ENCODER2_BUTTON_PIN);
  buttons = 0;
}