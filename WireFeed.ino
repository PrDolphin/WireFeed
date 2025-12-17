// Кнопка
#include "NumberButtons.hpp"
#include "InputAverager.hpp"
#include "TimerStopwatch.hpp"
#include "stepmotor.h"
#include <GyverTM1637.h>

#define MOTORS_SWITCH_PIN 2
#define TIMEOUT_LED_PIN 3
#define TIMER_START_STOP_PIN 6
#define TIMER_RESET_PIN 7
#define TIMER_STOPWATCH_SWITCH_PIN 11
#define COEF_ADD_PIN 12
#define COEF_SUB_PIN 8

#define EEPROM_C1_ADDRESS 0

/*------------------- Значение всех изменяемых величин -------------------*/
// Размотчик
#define D1 4 // Диаметр в см, <= 9
#define U1 16 // Отношение оборотов редуктора на оборот ШД

// Пистолет
#define D2 3
#define U2 5

#define POTENT_POLL_INTERVAL 100
#define ACCELERATION_INTERVAL 1

// Микрошаг
#define MICROSTEP 3200
// Делитель
#define PRESCALER 1

#define C1 1025

#define speed(a, x) ((a)/(x))

#define SPEED_TIME 0
#define ACCEL_TIME 1
#define TIMER_TIME 2
#define LED_TURNOFF_TIME 3

GyverTM1637 speed_disp(A1, A0);
GyverTM1637 timer_disp(A2, A0);
GyverTM1637 c1_disp(A3, A0);
InputAverager in_speed(A5, 507);
InputAverager in_time(A4, 610);
NumberButtons<int16_t> c1_off(COEF_ADD_PIN, COEF_SUB_PIN, EEPROM_C1_ADDRESS, 999);
TimerStopwatch<uint16_t> time_measurer(TIMER_START_STOP_PIN, TIMER_RESET_PIN, TIMER_STOPWATCH_SWITCH_PIN);
uint16_t lasttimes[LED_TURNOFF_TIME + 1];


// 3927 = 31416/8; 3 - 60/20
uint32_t motor_coef(uint8_t diameter, uint8_t reductor_multiplier, uint16_t c) {
  return ((uint32_t)(F_CPU*8/MICROSTEP/PRESCALER) * 3 * 3927 * diameter) / (reductor_multiplier * c);
}

void setup() {
  pinMode(MOTORS_SWITCH_PIN, INPUT_PULLUP);
  pinMode(TIMEOUT_LED_PIN, OUTPUT);
  pinMode(TIMER_START_STOP_PIN, INPUT_PULLUP);
  pinMode(TIMER_RESET_PIN, INPUT_PULLUP);
  pinMode(TIMER_STOPWATCH_SWITCH_PIN, INPUT_PULLUP);
  pinMode(COEF_ADD_PIN, INPUT_PULLUP);
  pinMode(COEF_SUB_PIN, INPUT_PULLUP);
  motors_init();
  speed_disp.clear();
  speed_disp.brightness(7);
  timer_disp.clear();
  timer_disp.brightness(7);
  if (!time_measurer.isTimer())
    timer_disp.displayInt(time_measurer.seconds);
  c1_disp.clear();
  c1_disp.brightness(7);
  c1_disp.displayInt(c1_off.number);
  motor_constants[0] = motor_coef(D1, U1, C1 + c1_off.number); // Для размотчика
  motor_constants[1] = motor_coef(D2, U2, 1000); // Для пистолета
}

void loop() {
  uint16_t time = millis();
  if (time - lasttimes[SPEED_TIME] < 0x8000) {
    bool system_enabled = !digitalRead(MOTORS_SWITCH_PIN);
    uint16_t speed = in_speed.get();
    speed_disp.displayInt(speed);
    target_speed = (system_enabled) ? speed : 0;
    lasttimes[SPEED_TIME] = time + POTENT_POLL_INTERVAL;
  }
  if (time - lasttimes[ACCEL_TIME] < 0x8000) {
    accelerate((uint8_t)(time & 0xFF) - lasttimes[ACCEL_TIME] + 1);
    lasttimes[ACCEL_TIME] = time + ACCELERATION_INTERVAL;
  }
  if (time - lasttimes[TIMER_TIME] < 0x8000) {
    time_measurer.startseconds = in_time.get();
    lasttimes[TIMER_TIME] = time + POTENT_POLL_INTERVAL;
    if (time_measurer.isTimer() && !time_measurer.isTicking() && time_measurer.seconds == 0)
      timer_disp.displayInt(time_measurer.startseconds);
  }
  switch (c1_off.tick(time)) {
    case NUMBER_CHANGED:
      c1_disp.displayInt(c1_off.number);
      break;
    case NUMBER_WRITTEN: {
      uint32_t constant = motor_coef(D1, U1, C1 + c1_off.number);
      if (accelerate(0) == 0 && target_speed != 0) { // If we going to recalculate speed anyway, don't waste cycles on it and don't interrupt acceleration
        uint16_t interval = speed(constant, target_speed);
        cli();
        motor_intervals[0] = interval;
      }
      cli();
      motor_constants[0] = constant;
      sei();
      break;
    }
  }
  switch (time_measurer.tick(time)) {
    case TIMERSTOPWATCH_MODE_CHANGED:
    case TIMERSTOPWATCH_RESET:
      if (time_measurer.isTimer())
        break;

    case TIMERSTOPWATCH_TICK:
      timer_disp.displayInt(time_measurer.seconds);
      if (time_measurer.isTimer() && time_measurer.seconds <= 4) {
        digitalWrite(TIMEOUT_LED_PIN, 1);
        lasttimes[LED_TURNOFF_TIME] = time + ((time_measurer.seconds == 1) ? 1000 : 100);
      }
  }
  if (time - lasttimes[LED_TURNOFF_TIME] < 0x8000) {
    digitalWrite(TIMEOUT_LED_PIN, 0);
  }
}

