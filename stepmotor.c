#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdint.h>
#include "stepmotor.h"

#define ENA_PIN (1 << 6) // PIN 13

#define MINSPEED 10
#define ACCELERATION_STEP 1

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

uint32_t motor_constants[2];
uint16_t motor_intervals[2] = {UINT16_MAX, UINT16_MAX};
uint16_t target_speed = 0;

void motors_init() {
  uint8_t sreg = SREG;
  cli();
  DDRB |= 0x06 | ENA_PIN; // enable PB1 and PB2 as output pins, enable ENA_PIN
  PORTB |= ENA_PIN;
  OCR1A = 0;
  OCR1B = 0;
  TCCR1A = 0;
  TCNT1 = 0;
  TCCR1B = 1 << CS10;
  TIMSK1 = (1 << OCIE1A) | (1 << OCIE1B);
  SREG = sreg;
}

#define speed(a, x) ((a)/(x))

uint16_t accelerate (uint8_t n_steps) {
  static uint16_t current_speed = 0;
  uint16_t local_target_speed = target_speed;
  if (local_target_speed == current_speed)
    return 0;
  if (n_steps == 0)
    return current_speed;
  n_steps = MIN(n_steps, 10);
  uint16_t local_intervals[2];
  uint8_t sreg = SREG;
  if (target_speed == 0) {
    if (current_speed <= MINSPEED) {
      sreg = SREG;
      cli();
      current_speed = 0;
      memset(motor_intervals, 0xFF, sizeof(motor_intervals));
      motors_disable_all();
      SREG = sreg;
      PORTB |= ENA_PIN;
      return 0;
    }
    local_target_speed = MINSPEED;
  }
  if (current_speed == 0) {
    PORTB &= ~ENA_PIN;
    current_speed = MINSPEED + (n_steps - 1) * ACCELERATION_STEP;
    sreg = SREG;
    cli();
    motors_enable_all();
    SREG = sreg;
  } else {
    if (local_target_speed > current_speed)
      current_speed = MIN(current_speed + ACCELERATION_STEP * n_steps, local_target_speed);
    else
      current_speed = MAX(current_speed - ACCELERATION_STEP * n_steps, local_target_speed);
  }
  local_intervals[0] = speed(motor_constants[0], current_speed);
  local_intervals[1] = speed(motor_constants[1], current_speed);
  sreg = SREG;
  cli();
  memcpy(motor_intervals, local_intervals, sizeof(motor_intervals));
  SREG = sreg;
  return current_speed;
}

ISR(TIMER1_COMPA_vect) {
  OCR1A += motor_intervals[0];
}

ISR(TIMER1_COMPB_vect) {
  OCR1B += motor_intervals[1];
}