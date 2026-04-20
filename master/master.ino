#include <Wire.h>
#include "messages.hpp"
#include "StepperMotor.hpp"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define STARTUP_DELAY 100

// Stationary feeder (feeder 1)
#define D1 4 // Diameter, cm, <= 9
#define R1 16 // Gear reduction

// Handheld feeder (feeder 2)
#define D2 3
#define R2 5

#define MICROSTEP 3200
#define PRESCALER 1

#define C1 1025
#define C2 1000

#define PIN_ENA1 9
#define PIN_PUL1 11
#define PIN_ENA2 4
#define PIN_PUL2 5

static void process_messages(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff, MessageTransceiver<8> &resend_to);
static void process_messages1(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff);
static void process_messages2(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff);

// 3927 = 31416/8; 3 - 60/20
uint32_t motor_coef(uint8_t diameter, uint8_t gear_reduction, uint16_t correction) {
  return ((uint32_t)(F_CPU*8/MICROSTEP/PRESCALER) * 3 * 3927 * diameter) / (gear_reduction * correction);
}

MessageTransceiver<8> msg_stream1(Serial1, process_messages1);
MessageTransceiver<8> msg_stream2(Serial2, process_messages2);
StepperMotor<uint16_t> motor1(OCR1A, TCNT1, TCCR1A, TCCR1B);
StepperMotor<uint16_t> motor2(OCR3A, TCNT3, TCCR3A, TCCR3B);

void serial_clear(HardwareSerial &serial) {
  while(serial.available()) {
    serial.read();
  }
}

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(10);
  Serial1.begin(9600);
  Serial1.setTimeout(10);
  Serial2.begin(9600);
  Serial2.setTimeout(10);
  motor1.begin(motor_coef(D1, R1, C1));
  motor2.begin(motor_coef(D2, R2, C2));
  delay(STARTUP_DELAY);
  serial_clear(Serial1);
  serial_clear(Serial2);
  pinMode(PIN_ENA1, OUTPUT);
  pinMode(PIN_PUL1, OUTPUT);
  pinMode(PIN_ENA2, OUTPUT);
  pinMode(PIN_PUL2, OUTPUT);
}

static void process_messages1(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff) {
  process_messages(msg_type, msg_body, time_diff, msg_stream2);
}

static void process_messages2(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff) {
  process_messages(msg_type, msg_body, time_diff, msg_stream1);
}

static void process_messages(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff, MessageTransceiver<8> &resend_to) {
  switch (msg_type) {
    case MSG_MOTORS_SET_SPEED:
      motor1.target_speed = msg_body;
      motor2.target_speed = msg_body;
      break;
    case MSG_COEF_UPDATE:
      motor1.set_coefficient(motor_coef(D1, R1, C1 + (int16_t)msg_body));
      break;
    default: {
      if (msg_type < MSG_FIRST_REDIRECT)
        break;
      resend_to.msg_queue(msg_type, msg_body);
      if (time_diff != 0) {
        resend_to.msg_mark_time(time_diff);
      }
    }
  }
}

void loop() {
  uint8_t time = millis();
  msg_stream1.msg_process();
  msg_stream2.msg_process();
  motor1.tick(time);
  motor2.tick(time);
  digitalWrite(PIN_ENA1, motor1.get_speed() == 0);
  digitalWrite(PIN_ENA2, motor2.get_speed() == 0);
}