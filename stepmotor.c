#ifndef STEPMOTOR_H
#define STEPMOTOR_H
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdint.h>

uint16_t motor_intervals[2] = {UINT16_MAX, UINT16_MAX};

void motors_init() {
  uint8_t sreg = SREG;
  cli();
  DDRB |= 0x06; // enable PB1 and PB2 as output pins
  OCR1A = 0;
  OCR1B = 0;
  TCCR1A = 0;
  TCNT1 = 0;
  TCCR1B = 1 << CS10;
  TIMSK1 = (1 << OCIE1A) | (1 << OCIE1B);
  SREG = sreg;
}

void motors_update_state() {
  uint8_t working_mask = 0;
  for (uint8_t i = 0; i < 2; ++i) {
    working_mask |= (motor_intervals[i] != UINT16_MAX) << (6 - i * 2);
  }
  uint8_t changed = working_mask ^ TCCR1A;
  working_mask &= changed;
  TCCR1A ^= changed; // Enable
  if (!working_mask)
    return;
  uint8_t sreg = SREG;
  cli();
  if (working_mask & (1 << COM1A0))
    OCR1A = motor_intervals[0] + TCNT1;
  if (working_mask & (1 << COM1B0))
    OCR1B = motor_intervals[1] + TCNT1;
  SREG = sreg;
}

ISR(TIMER1_COMPA_vect) {
  OCR1A += motor_intervals[0];
}

ISR(TIMER1_COMPB_vect) {
  OCR1B += motor_intervals[1];
}

#endif // STEPMOTOR_H