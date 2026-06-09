#include "common.h"
#include "contpcb_lib.h"

#include "NumberButtons.hpp"

#include <ModbusSerial.h>
#include <GyverTM1637.h>

#define ENCODER_CONNECTED 1
#define ENCODER_SHIFT_COEF 10
#define POLL_INTERVAL 5
#define SHOW_TIMERSTARTTIME_MS 3000
#define BUTTON_ENCODER ENCODER_CONNECTED
#define TIMER_STOPWATCH_SELECTOR 4
#define BUTTON_COEF_PLUS ARR_BUT4
#define BUTTON_COEF_MINUS ARR_BUT5
#define STARTUP_DELAY 100
#define DISPLAY_REFRESH_INTERVAL 250

uint16_t update_time = 0;
uint16_t display_refresh_time = 0;
uint16_t last_displayed_time = 0;

GyverTM1637 coef_disp(SOFTI2C_CLK, SOFTI2C_DIO);
GyverTM1637 timer_disp(SOFTI2C_CLK2, SOFTI2C_DIO);

NumberButtons<int16_t> coef(BUTTON_COEF_PLUS, BUTTON_COEF_MINUS, 999);

ModbusSerial mb (Serial, MODBUS_ADDRESS2, REDE_PIN);

void display_time(uint16_t time) {
  uint8_t data[4];
  uint16_t temp;
  data[3] = time % 10;
  temp = time / 10;
  data[2] = temp % 6;
  temp /= 6;
  data[1] = temp % 10;
  temp /= 10;
  data[0] = temp % 10;
  for (uint8_t i = 0; i < sizeof(data); ++i) {
    data[i] = digToHEX(data[i]);
  }
  data[1] |= 0x80; // Enable point
  timer_disp.displayByte(data);
}

void setup() {
  pcb_init(ENCODER_CONNECTED + 1);
  Serial.begin(MODBUS_BAUDRATE);
  Serial.setTimeout(10);
  mb.config(MODBUS_BAUDRATE);
  for (uint8_t i = 0; i < MODBUS_COILS2; ++i)
    mb.addCoil(i, 0);
  for (uint8_t i = 0; i < MODBUS_HREGS2; ++i)
    mb.addHreg(i, 0);
  coef_disp.clear();
  coef_disp.brightness(7);
  timer_disp.clear();
  timer_disp.brightness(7);
  while (mb.coil(MODBUS_COIL_CONTROLLER_INIT) == 0) {
    mb.task();
  }
  last_displayed_time = mb.hreg(MODBUS_HREG_TIME);
  display_time(last_displayed_time);
  coef.value = mb.hreg(MODBUS_HREG_COEF);
  coef_disp.displayInt(coef.value);
}

void loop() {
  mb.task();
  uint16_t time = millis();
  if (time - update_time >= POLL_INTERVAL) {
    update_time = time + POLL_INTERVAL;
    uint8_t changed = buttons;
    buttons_update();
    changed ^= buttons;
    if ((buttons >> TIMER_STOPWATCH_SELECTOR) & 0x1) {
      if ((changed >> TIMER_STOPWATCH_SELECTOR) & 0x1) {
        uint16_t timer_starttime = mb.hreg(MODBUS_HREG_TIMER_STARTTIME);
        cli();
        encoder_pos[ENCODER_CONNECTED] = timer_starttime;
        sei();
      } else {
        cli();
        uint16_t timer_starttime = encoder_pos[ENCODER_CONNECTED];
        sei();
        if (timer_starttime < 1 || timer_starttime > 5999) {
          timer_starttime = constrain((int16_t)timer_starttime, 1, 5999);
          cli();
          encoder_pos[ENCODER_CONNECTED] = timer_starttime;
          sei();
        }
        mb.setHreg(MODBUS_HREG_TIMER_STARTTIME, timer_starttime);
      }
    }
    if ((changed >> (BUTTON_ENCODER)) & 0x1) {
      encoder_step_multiplier[ENCODER_CONNECTED] = ((buttons >> (BUTTON_ENCODER)) & 0x1) ? ENCODER_SHIFT_COEF : 1;
    }
    mb.setCoil(MODBUS_COIL_TIMER_STOPWATCH_SELECT, (buttons >> TIMER_STOPWATCH_SELECTOR) & 0x1);

  }
  if (last_displayed_time != mb.hreg(MODBUS_HREG_TIME)) {
    last_displayed_time = mb.hreg(MODBUS_HREG_TIME);
    display_time(last_displayed_time);
  }
  if (coef.tick(time) == NUMBER_CHANGED) {
    mb.setHreg(MODBUS_HREG_COEF, coef.value);
    coef_disp.displayInt(mb.hreg(MODBUS_HREG_COEF));
  } else if (coef.value != mb.hreg(MODBUS_HREG_COEF)) {
    coef.value = mb.hreg(MODBUS_HREG_COEF);
    coef_disp.displayInt(mb.hreg(MODBUS_HREG_COEF));
  }
  if (time - display_refresh_time >= DISPLAY_REFRESH_INTERVAL) {
    display_refresh_time = time;
    display_time(last_displayed_time);
    coef_disp.displayInt(mb.hreg(MODBUS_HREG_COEF));
  }
}