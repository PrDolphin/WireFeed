// Кнопка
#include "NumberButtons.hpp"
#include "InputAverager.hpp"
#include "stepmotor.h"
#include "ui.hpp"

#define MOTORS_SWITCH_PIN 2
#define TIMEOUT_LED_PIN 3
#define TIMER_START_STOP_PIN 6
#define TIMER_RESET_PIN 7
#define TIMER_STOPWATCH_SWITCH_PIN 11
#define COEF_ADD_PIN 12
#define COEF_SUB_PIN 13

#define EEPROM_C1_ADDRESS 0

/*------------------- Значение всех изменяемых величин -------------------*/
// Размотчик
#define D1 4 // Диаметр в см, <= 9
#define U1 16 // Отношение оборотов редуктора на оборот ШД

// Пистолет
#define D2 3
#define U2 5

#define SPEED_POLL_INTERVAL 100
#define ACCELERATION_INTERVAL 1

// Микрошаг
#define MICROSTEP 3200
// Делитель
#define PRESCALER 1

#define C1 1025

#define speed(a, x) ((a)/(x))

#define SPEED_TIME 0
#define ACCEL_TIME 1

#define SPEED_DISP 0
#define TIMER_DISP 1
#define C1_DISP 2
#define DISPLAYS 3
#define DISPLAY_START_PIN A0

InputAverager in_speed(A5);
InputAverager in_time(A4);
NumberButtons<int16_t> c1_off(COEF_ADD_PIN, COEF_SUB_PIN, EEPROM_C1_ADDRESS, 999);
uint8_t lasttimes[ACCEL_TIME + 1];

// 3927 = 31416/8; 3 - 60/20
uint32_t motor_coef(uint8_t diameter, uint8_t reductor_multiplier, uint16_t c) {
  return ((uint32_t)(F_CPU*8/MICROSTEP/PRESCALER) * 3 * 3927 * diameter) / (reductor_multiplier * c);
}

void setup() {
  pinMode(MOTORS_SWITCH_PIN, INPUT_PULLUP);
  pinMode(TIMEOUT_LED_PIN, OUTPUT);
  pinMode(TIMER_START_STOP_PIN, INPUT_PULLUP);
  pinMode(TIMER_RESET_PIN, INPUT_PULLUP);
  pinMode(COEF_ADD_PIN, INPUT_PULLUP);
  pinMode(COEF_SUB_PIN, INPUT_PULLUP);
  motors_init();
  ui_setup(DISPLAYS, DISPLAY_START_PIN);
  disp_print(C1_DISP, c1_off.number);
  motor_constants[0] = motor_coef(D1, U1, C1 + c1_off.number); // Для размотчика
  motor_constants[1] = motor_coef(D2, U2, 1000); // Для пистолета
}

void loop() {
  uint16_t time = millis();
  if ((uint8_t)(time & 0xFF) - lasttimes[SPEED_TIME] < 0x80) {
    bool system_enabled = !digitalRead(MOTORS_SWITCH_PIN);
    //uint16_t speed = 200;
    uint16_t speed = map(in_speed.get(), 0, 1023, 10, 500);
    disp_print(SPEED_DISP, speed);
    target_speed = (system_enabled) ? speed : 0;
    lasttimes[SPEED_TIME] = time + SPEED_POLL_INTERVAL;
  }
  if ((uint8_t)(time & 0xFF) - lasttimes[ACCEL_TIME] < 0x80) {
    accelerate(ACCELERATION_INTERVAL);
    lasttimes[ACCEL_TIME] = time + ACCELERATION_INTERVAL;
  }
  switch (c1_off.tick(time)) {
    case NUMBER_CHANGED:
      disp_print(C1_DISP, c1_off.number);
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
}

