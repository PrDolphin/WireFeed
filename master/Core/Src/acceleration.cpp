#include "main.h"
#include "FreeRTOS.h"

#include "StepperMotor.hpp"
#include "communication.h"

#define TARGET_CLOCK 84000000u

// Handheld feeder (feeder 1)
#define D1 3
#define R1 5

// Stationary feeder (feeder 2)
#define D2 4 // Diameter, cm, <= 9
#define R2 16 // Gear reduction

#define MICROSTEP 3200
#define PRESCALER 1

#define C1 100461
#define C2 101665

// 3 = 60/(10*[2 - toggle compensation (timer overflows twice for 1 cycle)]);
static constexpr uint32_t motor_coef(unsigned diameter, unsigned gear_reduction, unsigned correction) {
  return ((uint64_t)(TARGET_CLOCK/MICROSTEP/PRESCALER) * 3 * 3141593 * diameter) / (gear_reduction * correction);
}

StepperMotor motor1{htim2, TIM_CHANNEL_1, motor_coef(D1, R1, C1), MOTORS_ENA_GPIO_Port, MOTORS_ENA_Pin};
StepperMotor motor2{htim5, TIM_CHANNEL_2, motor_coef(D2, R2, C2), MOTORS_ENA_GPIO_Port, MOTORS_ENA_Pin};

extern "C" void processAcceleration() {
  motor1.target_speed = (motor_enabled || managed_mode == 1) ? motors_target_speed : 0;
  motor2.target_speed = (motor_enabled || managed_mode == 1) ? motors_target_speed + speed_offset : 0;
  motor1.tick_IT();
  motor2.tick_IT();
}