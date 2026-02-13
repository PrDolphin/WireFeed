#include "definitions.h"
#include "contpcb_lib.h"

// https://forum.arduino.cc/t/resetting-millis-to-zero-reset-clock/180147

template <void (*fn)(void), typename T, T interval> void exec_timed(T time) {
  static T last_time = time;
  if (last_time - time < ((T)-1) >> 1)
    return;
  last_time += interval;
  fn();
}

extern volatile unsigned long timer0_millis;
volatile uint16_t &timer0_millis16 = *(volatile uint16_t*)&timer0_millis;

uint16_t millis16() {
  uint8_t sreg = SREG;
  uint16_t time;
  cli();
  time = *(uint16_t*)&timer0_millis;
  SREG = sreg;
  return time;
}

uint16_t buttons_sent;
uint16_t analog_sent[2];
int16_t encoder_pos_sent[2];

void setup() {
  
  pcb_init();
  //Serial.begin(9600);
}

void loop() {
  
}