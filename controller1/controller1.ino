#include "messages.hpp"
#include "contpcb_lib.h"
#include "TimerStopwatch.hpp"

#include <GyverTM1637.h>

#define ENCODER_CONNECTED 1
#define ENCODER_SHIFT_COEF 10
#define POLL_INTERVAL 100 // Prevent button bounce by simply not asking about it too often
#define BUTTON_ENCODER 0
#define MOTORS_SWITCHON 1
#define BUTTON_TIMER_STARTSTOP 2
#define BUTTON_TIMER_RESET 3
#define LED_PIN PWM_PIN1
#define LED_SHORT 300
#define LED_LONG 900
#define STARTUP_DELAY 1000

int16_t motorspeed_sent = 0;
uint16_t update_time = 0;
uint16_t led_shutdown_time = 0;
uint8_t buttons = 0;

GyverTM1637 speed_disp(SCL, SDA);
GyverTM1637 timer_disp(SOFTI2C_CLK, SOFTI2C_DIO);

TimerStopwatch<uint16_t> timerstopwatch;

static void process_messages(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff);

MessageTransceiver<8> msg_stream(Serial, process_messages);

void buttons_update() {
  buttons = (~PIND >> (ENC_BUT1 + ENCODER_CONNECTED) & 1) | (~PINC & 0xE); // Buttons are expected on A1, A2, A3
}

void serial_clear(HardwareSerial &serial) {
  while(serial.available()) {
    serial.read();
  }
}

void setup() {
  pcb_init(ENCODER_CONNECTED + 1);
  Serial.begin(9600);
  Serial.setTimeout(10);
  update_time = millis();
  buttons_update();
  buttons = ~buttons; // To resend entire state
  speed_disp.clear();
  speed_disp.brightness(7);
  timer_disp.clear();
  timer_disp.brightness(7);
  timer_disp.displayClock(timerstopwatch.seconds / 60, timerstopwatch.seconds % 60);
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
      timer_disp.displayClock(timerstopwatch.seconds / 60, timerstopwatch.seconds % 60);
      break;
    case MSG_TIMERSTOPWATCH_START:
      timerstopwatch.start(msg_body + time_diff);
      break;
    case MSG_TIMERSTOPWATCH_TIMER_SECONDS:
      if (msg_body == 0)
        timerstopwatch.setmode_stopwatch();
      else
        timerstopwatch.setmode_timer(msg_body);
      timer_disp.displayClock(timerstopwatch.seconds / 60, timerstopwatch.seconds % 60);
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
      timer_disp.displayClock(timerstopwatch.seconds / 60, timerstopwatch.seconds % 60);
    }
  }
  if (timerstopwatch.tick(time)) {
    timer_disp.displayClock(timerstopwatch.seconds / 60, timerstopwatch.seconds % 60);
    if (timerstopwatch.timer() && timerstopwatch.seconds <= 3) {
      digitalWrite(LED_PIN, 1);
      led_shutdown_time = time + ((timerstopwatch.seconds == 1) ? LED_LONG : LED_SHORT);
    }
  }
  if (time - led_shutdown_time < 0x8000) {
    digitalWrite(LED_PIN, 0);
  }
}
