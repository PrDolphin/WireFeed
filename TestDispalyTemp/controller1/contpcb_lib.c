#include <Arduino.h>
#include <stdint.h>

#define ENCODER1_DIR_PIN 4
#define ENCODER2_DIR_PIN 5
#define ENCODER1_BUTTON_PIN 6
#define ENCODER2_BUTTON_PIN 7
// Pins A0, A1, A2, A3
#define BUTTON_ARRAY_PC_MASK (_BV(0) | _BV(1) | _BV(2) | _BV(3))
#define BUTTON_ARRAY_PB_MASK (_BV(3) | _BV(4))
#define ANALOG_SAMPLE_CONST 3 /* 1/8 */
#define SOFTI2C_CLK 0
#define SOFTI2C_DIO_CLK2 7
#define PWM1 1
#define PWM2 2

#define ENCODER1_CONNECTED 0x1
#define ENCODER2_CONNECTED 0x2

uint16_t analog_data[2] = {0};
volatile int16_t encoder_pos[2] = {0};
uint8_t encoder_step_multiplier[2] = {1, 1};
uint8_t buttons = 0;
uint8_t buttons_debounce[7];

ISR(INT0_vect) {
  encoder_pos[0] += (((PIND >> (ENCODER1_DIR_PIN - 1)) & 2) - 1) * encoder_step_multiplier[0];
}

ISR(INT1_vect) {
  encoder_pos[1] += (((PIND >> (ENCODER2_DIR_PIN - 1)) & 2) - 1) * encoder_step_multiplier[1];
}

void buttons_update() {
  uint8_t newbuttons = (~PIND >> (ENCODER1_BUTTON_PIN) & 0x3) | (~PINB >> (3 - 2) & 0xC) | (~PINC & 0x7) << 4;
  for (uint8_t i = 0; i < sizeof(buttons_debounce); ++i) {
    buttons_debounce[i] = (buttons_debounce[i] << 1) | ((newbuttons >> i) & 1);
    if (buttons_debounce[i] == 0)
      buttons &= ~(1 << i);
    if (buttons_debounce[i] == (uint8_t)-1)
      buttons |= (1 << i);
  }
}

uint16_t analog_read(uint8_t pin) {
  uint16_t value = analogRead(A7);
  if (pin > A0 && pin < A6) {
    return value;
  }
  pin -= (pin >= A6) ? A6 : 0;
  analog_data[pin] = (value << (6-ANALOG_SAMPLE_CONST)) + (analog_data[1] - (analog_data[1] >> ANALOG_SAMPLE_CONST));
  return (analog_data[pin] + (1 << (6 - 1))) >> 6; // Correct rounding
}

#define BV(x) (1 << (x))

void analog_write(uint8_t pin, uint16_t duty) {
  /* If timer runs in Normal or CTC mode, toggle pin instead of setting/clearing it */
  uint8_t is_normal_mode = (TCCR1A & (_BV(WGM11) | _BV(WGM10))) | (TCCR1B & (_BV(WGM13) | _BV(WGM12)));
  is_normal_mode = ((is_normal_mode & ~_BV(WGM12)) == 0) || (is_normal_mode == (_BV(WGM13) | _BV(WGM12)));
  switch (pin) {
    case 0:
    case 9:
      TCCR1A |= _BV(COM1A1) >> is_normal_mode;
      OCR1A = duty;
      break;
    case 1:
    case 10:
      TCCR1A |= _BV(COM1B1) >> is_normal_mode;
      OCR1B = duty;
      break;
  }
}

void analog_write_stop(uint8_t pin) {
  pin -= (pin >= 9) ? 9 : 0;
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

void pcb_init(uint8_t flags) {
  uint8_t sreg = SREG;
  cli();
  memset(encoder_pos, 0, sizeof(encoder_pos));
  SREG = sreg;
  memset(analog_data, 0 ,sizeof(analog_data));
  DDRB |= _BV(PWM1) | _BV(PWM2);
  PORTB |= BUTTON_ARRAY_PB_MASK;
  PORTC |= BUTTON_ARRAY_PC_MASK;
  PORTD |= _BV(ENCODER1_BUTTON_PIN) | _BV(ENCODER2_BUTTON_PIN) | _BV(2) | _BV(3);
  // INT0 and INT1
  EICRA = (_BV(ISC11) | _BV(ISC01));
  if (flags & ENCODER1_CONNECTED)
    EIMSK |= (_BV(INT0));
  if (flags & ENCODER2_CONNECTED)
    EIMSK |= (_BV(INT1));
  buttons = (~PIND >> (ENCODER1_BUTTON_PIN) & 0x3) | (~PINB >> (3 - 2) & 0xC) | (~PINC & 0xF) << 4;
  for (uint8_t i = 0; i < 8; ++i) {
    buttons_debounce[i] = -((buttons >> i) & 1);
  }
}