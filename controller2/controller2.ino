#include "messages.hpp"
#include "contpcb_lib.h"
#include "TimerStopwatch.hpp"

#include "NumberButtons.hpp"

#include <GyverTM1637.h>

#define ENCODER_CONNECTED 1
#define ENCODER_SHIFT_COEF 10
#define POLL_INTERVAL 100 // Prevent button bounce by simply not asking it too often
#define SHOW_TIMERSTARTTIME_MS 3000
#define TIMER_STOPWATCH_SELECTOR 1
#define BUTTON_COEF_PLUS ARR_BUT5
#define BUTTON_COEF_MINUS ARR_BUT6
#define BUTTON_ENCODER 0
#define STARTUP_DELAY 1000

uint16_t timer_starttime_sent = 1;
uint16_t update_time = 0;
bool timer_startvalue_displayed = false;
uint16_t timer_startvalue_last_updated = 0;
uint8_t buttons = 0;

GyverTM1637 coef_disp(SOFTI2C_CLK, SOFTI2C_DIO);
GyverTM1637 timer_disp(SCL, SDA);

TimerStopwatch<uint16_t> timerstopwatch;
NumberButtons<int16_t> coef(BUTTON_COEF_PLUS, BUTTON_COEF_MINUS, 4, 999);

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
  coef_disp.clear();
  coef_disp.brightness(7);
  coef_disp.displayInt(coef.value);
  timer_disp.clear();
  timer_disp.brightness(7);
  buttons_update();
  buttons = ~buttons; // To resend entire state
  delay(STARTUP_DELAY);
  serial_clear(Serial);
}

static void process_messages(uint8_t msg_type, uint16_t msg_body, uint16_t time_diff) {
  switch (msg_type) {
    case MSG_TIMERSTOPWATCH_STOP:
      timerstopwatch.stop(msg_body);
      timer_disp.displayClock(timerstopwatch.seconds / 60, timerstopwatch.seconds % 60);
      break;
    case MSG_TIMERSTOPWATCH_START:
      timerstopwatch.start(msg_body + time_diff);
      break;
    case MSG_TIMERSTOPWATCH_TIMER_SECONDS:
      if (msg_body == 0) {
        timerstopwatch.setmode_stopwatch();
        timer_disp.displayClock(timerstopwatch.seconds / 60, timerstopwatch.seconds % 60);
      } else {
        cli();
        encoder_pos[ENCODER_CONNECTED] = msg_body;
        sei();
        timerstopwatch.setmode_timer(msg_body);
        timer_startvalue_displayed = true;
        timer_startvalue_last_updated = millis();
        timer_disp.displayClock(msg_body / 60, msg_body % 60);
      }
    case MSG_COEF_UPDATE:
      coef.value = msg_body;
      coef_disp.displayInt(msg_body);
      break;
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
    uint16_t timer_starttime = encoder_pos[ENCODER_CONNECTED];
    sei();
    if (timer_starttime < 1 || timer_starttime > 5999) {
      timer_starttime = constrain((int16_t)timer_starttime, 1, 5999);
      cli();
      encoder_pos[ENCODER_CONNECTED] = timer_starttime;
      sei();
    }
    if ((changed >> (BUTTON_ENCODER)) & 0x1) {
      encoder_step_multiplier[ENCODER_CONNECTED] = ((buttons >> (BUTTON_ENCODER)) & 0x1) ? ENCODER_SHIFT_COEF : 1;
    }
    if (((changed >> TIMER_STOPWATCH_SELECTOR) & 0x1) || timer_starttime_sent != timer_starttime) {
      if ((buttons >> TIMER_STOPWATCH_SELECTOR) & 0x1) {
        timerstopwatch.setmode_timer(timer_starttime);
        msg_stream.msg_queue(MSG_TIMERSTOPWATCH_TIMER_SECONDS, timer_starttime);
      } else if ((changed >> TIMER_STOPWATCH_SELECTOR) & 0x1) {
        timerstopwatch.setmode_stopwatch();
        msg_stream.msg_queue(MSG_TIMERSTOPWATCH_TIMER_SECONDS, 0);
      } 
      if (timer_starttime_sent != timer_starttime) {
        timer_startvalue_displayed = true;
        timer_startvalue_last_updated = time;
        timer_starttime_sent = timer_starttime;
        timer_disp.displayClock(timer_starttime / 60, timer_starttime % 60);
      } else {
        timer_disp.displayClock(timerstopwatch.seconds / 60, timerstopwatch.seconds % 60);
      }
    }
  }
  if (timer_startvalue_displayed) {
    timerstopwatch.tick(time);
    if (time - timer_startvalue_last_updated > SHOW_TIMERSTARTTIME_MS) {
      timer_startvalue_displayed = false;
      timer_disp.displayClock(timerstopwatch.seconds / 60, timerstopwatch.seconds % 60);
    }
  } else if (timerstopwatch.tick(time)) {
    timer_disp.displayClock(timerstopwatch.seconds / 60, timerstopwatch.seconds % 60);
  }
  if (coef.tick(time) == NUMBER_CHANGED) {
    msg_stream.msg_queue(MSG_COEF_UPDATE, coef.value);
    coef_disp.displayInt(coef.value);
  }
}