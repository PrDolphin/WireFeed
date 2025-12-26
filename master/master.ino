#include <Wire.h>
#include "definitions.h"

struct test {
  uint32_t a;
  uint16_t b;
  uint8_t d;
  uint32_t c;
};

void setup() {
  Wire.begin();
  Wire.setClock(I2C_FREQUENCY);
  Serial.begin(9600);
  Serial.println(sizeof(test));
  //pinMode(A4, INPUT);
  //pinMode(A5, INPUT);
}

void loop() {
  uint8_t size = Wire.requestFrom(CONTROLLER1_ADDRESS, 2);
  union {
    int16_t val;
    uint8_t dat[2];
  } position;
  for (uint8_t i = 0; i < size; ++i) {
    position.dat[i] = Wire.read();
  }
  Serial.println(position.val);
  delay(200);
}