#include <stdint.h>

#include <Arduino.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#define UPDATE_TIME 1 // MS
#define MINSPEED 1
#define ACCELERATION_STEP 1

#define speed(a, x) ((a) / (x))

template <typename timer_type>
class StepperMotor {
private:
  uint32_t coefficient = 0;
  uint16_t speed = 0;
  uint8_t last_updated = 0;
  volatile timer_type &timer_max;
  volatile timer_type &timer;
  volatile uint8_t &tccr_a;
  volatile uint8_t &tccr_b;
  void start_motor() {
    tccr_b |= _BV(CS10);
  }
  void stop_motor() {
    tccr_b &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));
    timer = 0;
  }
public:
  uint16_t target_speed;
  StepperMotor(volatile timer_type &oc_a, volatile timer_type &tcnt, volatile uint8_t &tccr_a, volatile uint8_t &tccr_b)
  : timer_max{oc_a}, timer{tcnt}, tccr_a{tccr_a}, tccr_b{tccr_b} {
  }
  void begin(uint32_t coefficient) {
    this->coefficient = coefficient;
    uint8_t sreg = SREG;
    cli();
    tccr_a = _BV(COM1A0) | _BV(WGM11) | _BV(WGM10); // Using mode 15 for toggle behavior with OCnA buffering
    tccr_b = _BV(WGM12) | _BV(WGM13);
    timer = 0;
    SREG = sreg;
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
    uint8_t sreg = SREG;
    cli();
    timer_max = speed(coefficient, speed);
    SREG = sreg;
    return;
  }

  uint16_t get_speed() {
    return speed;
  }

  void set_coefficient(uint32_t coef) {
    uint8_t sreg = SREG;
    cli();
    if (speed > 0)
      speed = (coef + timer_max / 2) / timer_max;
    coefficient = coef;
    SREG = sreg;
  }
};