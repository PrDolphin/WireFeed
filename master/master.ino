#include <Wire.h>
#include "messages.hpp"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define STARTUP_DELAY 1000

static void process_messages(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff, MessageTransceiver<8> &resend_to);
static void process_messages1(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff);
static void process_messages2(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff);

MessageTransceiver<8> msg_stream1(Serial1, process_messages1);
MessageTransceiver<8> msg_stream2(Serial2, process_messages2);

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
  delay(STARTUP_DELAY);
  serial_clear(Serial1);
  serial_clear(Serial2);
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
      // something
      break;
    case MSG_COEF_UPDATE:
      // something
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
  msg_stream1.msg_process();
  msg_stream2.msg_process();
}