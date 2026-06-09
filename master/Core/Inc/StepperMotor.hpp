#ifndef ACCELERATION_H
#define ACCELERATION_H

#include <stdint.h>
#include "main.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define UPDATE_TIME 1 // MS
#define MINSPEED 1
#define ACCELERATION_STEP 1

#define speed(a, x) ((a) / (x))

template <int a = 1>
class StepperMotor {
  private:
  uint32_t coefficient = 0;
  uint16_t speed = 0;
  uint8_t last_updated = 0;
  TIM_HandleTypeDef &timer;
  uint32_t channel;
  void start_motor() {
    HAL_GPIO_WritePin(ena_port, ena_pin, GPIO_PIN_SET);
    HAL_TIM_OC_Start(&timer, channel);
  }
  void stop_motor() {
    HAL_TIM_OC_Stop(&timer, channel);
    HAL_GPIO_WritePin(ena_port, ena_pin, GPIO_PIN_RESET);
    timer.Instance->CNT = 0;
  }
public:
  uint16_t target_speed = 0;
  GPIO_TypeDef *ena_port;
  uint32_t ena_pin;
  StepperMotor(TIM_HandleTypeDef &timer, uint32_t channel, uint32_t coefficient, GPIO_TypeDef *ena_port, uint32_t ena_pin)
    : timer{timer}, channel{channel}, coefficient{coefficient}, ena_port{ena_port}, ena_pin{ena_pin} {
  }
  /*void begin(uint32_t coefficient) {
    this->coefficient = coefficient;
    uint8_t sreg = SREG;
    cli();
    tccr_a = _BV(COM1A0) | _BV(WGM11) | _BV(WGM10); // Using mode 15 for toggle behavior with OCnA buffering
    tccr_b = _BV(WGM12) | _BV(WGM13);
    timer = 0;
    SREG = sreg;
  }*/
  void tick_IT() {
    uint16_t local_target_speed = target_speed;
    if (local_target_speed == speed)
      return;
    if (target_speed == 0) {
      if (speed <= MINSPEED) {
        speed = 0;
        stop_motor();
        return;
      }
      local_target_speed = MINSPEED;
    }
    if (speed == 0) {
      speed = MINSPEED;
      timer.Instance->ARR = coefficient;
      start_motor();
      return;
    } else {
      if (local_target_speed > speed)
        speed = MIN(speed + ACCELERATION_STEP, local_target_speed);
      else
        speed -= ACCELERATION_STEP;
    }
    timer.Instance->ARR = speed(coefficient, speed);
    return;
  }
  void tick(uint8_t time_ms) {
    uint16_t local_target_speed = target_speed;
    if (local_target_speed == speed) {
      last_updated = time_ms;
      return;
    }
    uint8_t n_steps = (time_ms - last_updated) / UPDATE_TIME;
    if (n_steps == 0)
      return;
    n_steps = MIN(n_steps, 10);
    last_updated += n_steps * UPDATE_TIME;
    if (target_speed == 0) {
      if (speed <= MINSPEED) {
        speed = 0;
        stop_motor();
        return;
      }
      local_target_speed = MINSPEED;
    }
    if (speed == 0) {
      speed = MINSPEED + MIN((n_steps - 1) * ACCELERATION_STEP, local_target_speed - MINSPEED);
      start_motor();
    } else {
      if (local_target_speed > speed)
        speed = MIN(speed + ACCELERATION_STEP * n_steps, local_target_speed);
      else
        speed = (ACCELERATION_STEP * n_steps < speed - local_target_speed)
                  ? speed - ACCELERATION_STEP * n_steps
                  : local_target_speed;
    }
    timer.Instance->ARR = speed(coefficient, speed);
    return;
  }

  uint16_t get_speed() {
    return speed;
  }

  void set_coefficient(uint32_t coef) {
    if (coefficient == coef)
      return;
    if (speed > 0) {
      uint32_t timermax = timer.Instance->ARR;
      speed = (coef + timermax / 2) / timermax;
    }
    coefficient = coef;
  }
};

#endif // ACCELERATION_H