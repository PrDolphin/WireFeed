#include <Arduino.h>
#include <GyverTM1637.h>

GyverTM1637 *displays = nullptr;

const uint8_t movingAvgSize = 20;
uint16_t movingAvg[movingAvgSize] = {0};

void disp_print(uint8_t display, int16_t value) {
  displays[display].displayInt(value);
#ifdef DEBUG
  Serial.print(display);
  Serial.print(": ");
  Serial.println(value);
#endif
  /*uint16_t abs_offset = abs(offset);
  uint8_t digits[3];
  digits[0] = abs_offset / 100;
  uint16_t tmp = digits[0] * 100;
  digits[1] = (abs_offset - tmp) / 10;
  tmp += digits[1] * 10;
  digits[2] = (abs_offset - tmp);
  // 0x40 is for '-' in GyverTM1637 library
  coefdisp.displayByte(0x80 | (offset < 0) << 6, digToHEX(digits[0]), digToHEX(digits[1]), digToHEX(digits[2])); */// Always displays the point, because fraction
}

void ui_setup(uint8_t n_displays, uint8_t startpin) {
#ifdef DEBUG
  Serial.begin(9600);
  Serial.setTimeout(5);
#endif
  if (displays != nullptr)
    free(displays);
  displays = (GyverTM1637*)malloc(n_displays * sizeof(displays[0]));
  for (uint8_t i = 0; i < n_displays; ++i) {
    GyverTM1637 display(startpin + 1 + i, startpin); // Doesn't have dedicated destructor (and never will)
    memcpy(displays, &display, sizeof(GyverTM1637)); // Working around lack of placement new in arduino C++ lib
    display.clear();
    display.brightness(7);
  }
}