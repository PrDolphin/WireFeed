#include "main.h"
#include "FreeRTOS.h"

#include "StepperMotor.hpp"

#define TARGET_CLOCK 84000000u

// Stationary feeder (feeder 1)
#define D1 4 // Diameter, cm, <= 9
#define R1 16 // Gear reduction

// Handheld feeder (feeder 2)
#define D2 3
#define R2 5

#define MICROSTEP 6400
#define PRESCALER 1

#define C1 1025
#define C2 1000

// 3927 = (3.1416*10000)/8; 3 = 60/(10*[2 - toggle compensation (timer overflows twice for 1 cycle)]); correction = 1000+-smth
static constexpr uint32_t motor_coef(uint8_t diameter, uint8_t gear_reduction, uint16_t correction) {
  return ((uint32_t)(TARGET_CLOCK*8/MICROSTEP/PRESCALER) * 3 * 3927 * diameter) / (gear_reduction * correction);
}

StepperMotor motor1{htim2, TIM_CHANNEL_1, motor_coef(D1, R1, C1), MOTORS_ENA_GPIO_Port, MOTORS_ENA_Pin};
StepperMotor motor2{htim5, TIM_CHANNEL_2, motor_coef(D2, R2, C2), MOTORS_ENA_GPIO_Port, MOTORS_ENA_Pin};

extern "C" void processAcceleration() {
  motor1.tick_IT();
  motor2.tick_IT();
}

extern "C" void setSpeed(uint16_t speed) {
  motor1.target_speed = speed;
  motor2.target_speed = speed;
}

extern "C" void setCoefOff2(int16_t coef_off) {
  motor1.set_coefficient(motor_coef(D1, R1, C1 + coef_off));
}