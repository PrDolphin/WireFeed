#include <Arduino.h>

#include "InputAverager.hpp"

uint16_t InputAverager::get() {
  uint16_t sum=0;
  for(uint8_t i = 0; i < ANALOG_INPUT_AVERAGE_FACTOR - 1; ++i) {
    avg[i] = avg[i + 1];
    sum += avg[i];
  }
  avg[ANALOG_INPUT_AVERAGE_FACTOR - 1] = analogRead(pin + 13);
  sum += avg[ANALOG_INPUT_AVERAGE_FACTOR - 1];
  return (sum + (ANALOG_INPUT_AVERAGE_FACTOR/2)) / ANALOG_INPUT_AVERAGE_FACTOR;
}