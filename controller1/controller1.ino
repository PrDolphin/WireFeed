#include <Wire.h>
#include "definitions.h"

#define ENCODER_INT_PIN 2
#define ENCODER_DIR_PIN 6

union {
  int16_t val;
  uint8_t dat[2];
} encoder_pos = {.val = 0};

volatile uint16_t last_checked = 0;

void onEncoderTurn(void) {
  uint16_t time = millis();
  if (time - last_checked < 50)
    return;
  last_checked = time;
  encoder_pos.val += (digitalRead(ENCODER_DIR_PIN) << 1) - 1;
}

void I2C_TxHandler(void)
{
  Wire.write(encoder_pos.dat[0]);
  Wire.write(encoder_pos.dat[1]);
}

void setup() {
  Wire.begin(CONTROLLER1_ADDRESS);
  Wire.setClock(I2C_FREQUENCY);
  Wire.onRequest(I2C_TxHandler);
  pinMode(2, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  pinMode(A4, INPUT);
  pinMode(A5, INPUT);
  attachInterrupt(digitalPinToInterrupt(ENCODER_INT_PIN), onEncoderTurn, FALLING);
  Serial.begin(9600);
}

void loop() {
  Serial.println(encoder_pos.val);
  delay(200);
}