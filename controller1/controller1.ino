#include "messages.hpp"
#include "contpcb_lib.h"
#include "TimerStopwatch.hpp"

#include <GyverTM1637.h>
#include <EEPROM.h>

#define ENCODER_CONNECTED 1
#define ENCODER_SHIFT_COEF 30
#define POLL_INTERVAL 5
#define BUTTON_ENCODER ENCODER_CONNECTED
#define MOTORS_SWITCHON 5
#define BUTTON_TIMER_STARTSTOP 6
#define BUTTON_TIMER_RESET 7
#define LED_PIN PWM_PIN1
#define LED_SHORT 300
#define LED_LONG 900
#define STARTUP_DELAY 100
#define DISPLAY_REFRESH_INTERVAL 250
#define SPEED_EEPROM_OFFSET 0
#define SPEED_SAVE_DELAY 1000

int16_t motorspeed_sent = 0;
uint16_t update_time = 0;
uint16_t led_shutdown_time = 0;
uint16_t display_refresh_time = 0;
uint8_t speed_need_save = 0;
uint16_t speed_save_time = 0;

GyverTM1637 speed_disp(SOFTI2C_CLK, SOFTI2C_DIO);
GyverTM1637 timer_disp(SOFTI2C_CLK2, SOFTI2C_DIO);

TimerStopwatch<uint16_t> timerstopwatch;

static void process_messages(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff);

MessageTransceiver<8> msg_stream(Serial, process_messages);

void serial_clear(HardwareSerial &serial) {
  while(serial.available()) {
    serial.read();
  }
}

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
  Serial.begin(9600);
  Serial.setTimeout(10);
  update_time = millis();
  buttons_update();
  speed_disp.clear();
  speed_disp.brightness(7);
  timer_disp.clear();
  timer_disp.brightness(7);
  display_time(timerstopwatch.seconds);
  uint16_t speed;
  for (uint8_t i = 0; i < sizeof(speed); ++i)
    speed |= (uint16_t)EEPROM.read(SPEED_EEPROM_OFFSET + i) << (i * 8);
  cli();
  encoder_pos[ENCODER_CONNECTED] = speed;
  sei();
  delay(STARTUP_DELAY);
  serial_clear(Serial);
}

static void process_messages(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff) {
  switch (msg_type) {
    case MSG_MOTORS_SET_SPEED:
      if (msg_body == 0)
        break; // we cannot programmatically move the switch
      cli();
      encoder_pos[ENCODER_CONNECTED] = msg_body;
      sei();
      speed_disp.displayInt(msg_body);
      break;
    case MSG_TIMERSTOPWATCH_STOP:
      timerstopwatch.stop(msg_body);
      display_time(timerstopwatch.seconds);
      break;
    case MSG_TIMERSTOPWATCH_START:
      timerstopwatch.start(msg_body + time_diff);
      break;
    case MSG_TIMERSTOPWATCH_TIMER_SECONDS:
      if (msg_body == 0)
        timerstopwatch.setmode_stopwatch();
      else
        timerstopwatch.setmode_timer(msg_body);
      display_time(timerstopwatch.seconds);
    default:
      break;
  }
}

void loop() {
  msg_stream.msg_process();
  uint16_t time = millis();
  if (time - update_time >= POLL_INTERVAL) {
    update_time = time + POLL_INTERVAL;
    uint8_t changed = buttons;
    buttons_update();
    changed ^= buttons;
    cli();
    uint16_t motorspeed = encoder_pos[ENCODER_CONNECTED];
    sei();
    if ((int16_t)motorspeed < 1) {
      cli();
      encoder_pos[ENCODER_CONNECTED] = 1;
      sei();
      motorspeed = 1;
    }
    if ((changed >> (BUTTON_ENCODER)) & 0x1) {
      encoder_step_multiplier[ENCODER_CONNECTED] = ((buttons >> (BUTTON_ENCODER)) & 0x1) ? ENCODER_SHIFT_COEF : 1;
    }
    if ((changed >> MOTORS_SWITCHON) & 0x1 || motorspeed != motorspeed_sent) {
      if ((buttons >> (MOTORS_SWITCHON)) & 0x1) {
        msg_stream.msg_queue(MSG_MOTORS_SET_SPEED, motorspeed);
      } else {
        if ((changed >> MOTORS_SWITCHON) & 0x1) {
          msg_stream.msg_queue(MSG_MOTORS_SET_SPEED, 0);
        }
      }
      if (motorspeed != motorspeed_sent) {
        speed_need_save = 1;
        speed_save_time = time;
        motorspeed_sent = motorspeed;
        speed_disp.displayInt(motorspeed);
      }
    }
    if ((((changed & buttons) >> BUTTON_TIMER_STARTSTOP) & 0x1)) {
      // Pressing timer start-stop button
      if (timerstopwatch.ticking()) {
        timerstopwatch.stop();
        msg_stream.msg_queue(MSG_TIMERSTOPWATCH_STOP, timerstopwatch.seconds);
      } else {
        timerstopwatch.start(time);
        msg_stream.msg_queue(MSG_TIMERSTOPWATCH_START, time);
        msg_stream.msg_mark_time();
      }
    }
    if ((((changed & buttons) >> BUTTON_TIMER_RESET) & 0x1)) {
      // Pressing timer reset button
      timerstopwatch.reset();
      msg_stream.msg_queue(MSG_TIMERSTOPWATCH_STOP, timerstopwatch.seconds);
      display_time(timerstopwatch.seconds);
    }
  }
  if (timerstopwatch.tick(time)) {
    display_time(timerstopwatch.seconds);
    if (timerstopwatch.timer() && timerstopwatch.seconds <= 3) {
      digitalWrite(LED_PIN, 1);
      led_shutdown_time = time + ((timerstopwatch.seconds == 1) ? LED_LONG : LED_SHORT);
    }
  }
  if (time - led_shutdown_time < 0x8000) {
    digitalWrite(LED_PIN, 0);
  }
  if (time - display_refresh_time >= DISPLAY_REFRESH_INTERVAL) {
    display_refresh_time = time;
    speed_disp.displayInt(motorspeed_sent);
    display_time(timerstopwatch.seconds);
  }
  if (speed_need_save && time - speed_save_time >= SPEED_SAVE_DELAY) {
    for (uint8_t i = 0; i < sizeof(motorspeed_sent); ++i) {
      uint8_t byte = EEPROM.read(SPEED_EEPROM_OFFSET + i);
      if (byte != motorspeed_sent >> (i * 8))
        EEPROM.write(SPEED_EEPROM_OFFSET + i, (uint8_t)(motorspeed_sent >> (i * 8)));
    }
  }
}
