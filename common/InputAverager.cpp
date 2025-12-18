#include <Arduino.h>

#include "InputAverager.hpp"

uint16_t InputAverager::get() {
  uint32_t sum = ANALOG_INPUT_AVERAGE_NEW / 2;
  uint16_t input;
  for (uint8_t i = 0; i < ANALOG_INPUT_AVERAGE_NEW; ++i) {
    sum += analogRead(pin);
  }
  input = sum * range / 1024 / ANALOG_INPUT_AVERAGE_NEW;
  sum = avg[0] + ANALOG_INPUT_AVERAGE_HISTORY / 2;
  for(uint8_t i = 0; i < ANALOG_INPUT_AVERAGE_HISTORY - 2; ++i) {
    avg[i] = avg[i + 1];
    sum += avg[i];
  }
  
  avg[ANALOG_INPUT_AVERAGE_HISTORY - 2] = input;
  sum += avg[ANALOG_INPUT_AVERAGE_HISTORY - 2];
  uint16_t result = sum / ANALOG_INPUT_AVERAGE_HISTORY;
  repeat_counter = min(repeat_counter + (result == last) * 2 - 1, (repeat_counter & 0x80) + ANALOG_INPUT_MIN_REPEATS);
  repeat_counter |= (repeat_counter >= ANALOG_INPUT_MIN_REPEATS - 1) << 7;
  if (repeat_counter > 0x80) {
    result = last;
  } else {
    repeat_counter &= ~0x80;
  }
  if (abs((int16_t)result - (int16_t)input) > ANALOG_INPUT_MAX_DELTA) {
    for(uint8_t i = 0; i < ANALOG_INPUT_AVERAGE_HISTORY - 1; ++i) {
      avg[i] = input;
    }
    result = input;
    repeat_counter = 0;
  }
  last = result;
  return result;
}