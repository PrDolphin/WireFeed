#define ENA_PIN 13

// Кнопка
#define BUT_PIN 3

#include "stepmotor.h"
#include "ui.hpp"

#define MINSPEED_CM_P_MIN 10
#define ACCELERATION_STEP_CM_P_MIN 1
#define ACCELERATION_STEP_TIME (F_CPU / 1000) // Every 1 ms

/*------------------- Значение всех изменяемых величин -------------------*/
// Размотчик
#define D1 4 // Диаметр в см
#define U1 16 // Отношение оборотов редуктора на оборот ШД

// Пистолет
#define D2 3
#define U2 5

// Микрошаг
#define MICROSTEP 3200
// Делитель
#define PRESCALER 1

#define C1 1.025
#define C2 1

// Вычисление коэффициентов 
const uint32_t K1 = (F_CPU * 60.0) / (PRESCALER * MICROSTEP * 2 * C1) * ((3.14 * D1) / U1); // Для размотчика
const uint32_t K2 = (F_CPU * 60.0) / (PRESCALER * MICROSTEP * 2 * C2) * ((3.14 * D2) / U2); // Для пистолета

// Новый формат скорости
// Уравнение перевода скорости вращения в такты
#define speed(a, x) ((a)/(x))

void accelerate (uint16_t target_speed) {
  static uint16_t current_speed = 0;
  uint16_t local_intervals[2];
  uint8_t sreg = SREG;
  if (current_speed == 0) {
    if (target_speed == 0) {
      return;
    }
    digitalWrite(ENA_PIN, LOW);
    current_speed = MINSPEED_CM_P_MIN;
    local_intervals[0] = speed(K1, MINSPEED_CM_P_MIN);
    local_intervals[1] = speed(K2, MINSPEED_CM_P_MIN);
    cli();
    memcpy(motor_intervals, local_intervals, sizeof(motor_intervals));
    motors_enable_all();
  }
  cli();
  uint16_t timer = TCNT1;
  SREG = sreg;
  bool engine_stop = false;
  if (target_speed == 0) {
    target_speed = MINSPEED_CM_P_MIN;
    engine_stop = true;
  }
  int16_t acceleration_sign = ACCELERATION_STEP_CM_P_MIN;
  if (target_speed < current_speed)
    acceleration_sign = -acceleration_sign;
  while (current_speed != target_speed) {
    current_speed += acceleration_sign;
    local_intervals[0] = speed(K1, current_speed);
    local_intervals[1] = speed(K2, current_speed);
    uint16_t timepassed;
    sreg = SREG;
    do {
      cli();
      timepassed = TCNT1; // Reading timer is atomic!
      SREG = sreg;
      timepassed -= timer;
    } while (timepassed < ACCELERATION_STEP_TIME);
    cli();
    memcpy(motor_intervals, local_intervals, sizeof(motor_intervals));
    SREG = sreg;
    timer += ACCELERATION_STEP_TIME;
  }
  if (engine_stop) {
    current_speed = 0;
    sreg = SREG;
    cli();
    memset(motor_intervals, 0xFF, sizeof(motor_intervals));
    motors_disable_all();
    SREG = sreg;
    delayMicroseconds(4); // Just in case
    digitalWrite(ENA_PIN, HIGH);
  }
}

void setup() {
  Serial.begin(9600); // Подключаем монитор
  Serial.setTimeout(5);
  pinMode(ENA_PIN, OUTPUT);
  pinMode(BUT_PIN, INPUT_PULLUP);
  digitalWrite(ENA_PIN, HIGH); // Выключаем шаговик
  motors_init();
  ui_setup();
}

void loop() {
  bool system_enabled = !digitalRead(BUT_PIN);
  uint16_t speed = get_speed();
  disp.displayInt(speed);
  accelerate((system_enabled) ? speed : 0);
  delay(10);
  Serial.println(speed);
}

