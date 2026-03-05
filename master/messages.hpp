#ifndef MESSAGES_HPP
#define MESSAGES_HPP

#include "crc4.h"

#define MSG_ACK 0
#define MSG_TIME 1
#define MSG_MOTORS_SET_SPEED 2
#define MSG_COEF_UPDATE 3
#define MSG_TIMERSTOPWATCH_STOP 4
#define MSG_TIMERSTOPWATCH_START 5
#define MSG_TIMERSTOPWATCH_TIMER_SECONDS 6
// First message to be ignored by main controller and instead retransmitted to slaves
#define MSG_FIRST_REDIRECT 3
#define MSG_SIZE 3

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define STATE_MSG_DISCARD 0x1
#define STATE_IDLE_RECEIVE 0x2
#define STATE_IDLE_SEND 0x4
#define STATE_ACK_AWAIT 0x8
#define STATE_MARK_SENT_TIME 0x10

#include <Arduino.h>
#define TIME_IDLE_RECEIVE 3
#define TIME_IDLE_SEND 1
#define ACK_TIMEOUT 10
#define RECEIVE_LATENCY_MS 3

template <uint8_t msgs_buffered = 8>
class MessageTransceiver {
private:
  uint8_t buf[msgs_buffered * MSG_SIZE];
  uint8_t buf_pos = 0;
  uint8_t buf_msgs = 0;
  uint8_t buf_pos_sent = 0;
  uint8_t state = STATE_IDLE_RECEIVE | STATE_IDLE_SEND;
  uint16_t last_read_activity = 0;
  uint16_t last_write_activity = 0;
  uint16_t mark_time_diff_from_localtime = 0;
  uint8_t last_serial_available_value = 0;
  uint8_t serial_write_available_max = 0;
  uint8_t received = 0;
  
  void msgs_discard() {
    while (serial.available()) {
      serial.read();
    }
  }
  void msg(uint8_t type, uint16_t body, uint8_t *buf) {
    buf[0] = type;
    memcpy(&buf[1], &body, sizeof(body));
    uint8_t crc = crc4(&buf[0], MSG_SIZE);
    buf[0] |= crc << 4;
  }
  void send_buffers(uint16_t time) {
    if (buf_msgs <= 0)
      return;
    if (state & STATE_MARK_SENT_TIME) {
      uint8_t lbuf[3];
      msg(MSG_TIME, time - mark_time_diff_from_localtime, lbuf);
      serial.write(lbuf, MSG_SIZE);
    }
    if (buf_msgs > buf_pos) // Printing from cyclic array
      serial.write(&buf[(buf_pos + msgs_buffered - buf_msgs) * MSG_SIZE], (buf_msgs - buf_pos) * MSG_SIZE);
    uint8_t start = (buf_pos > buf_msgs) ? (buf_pos - buf_msgs) * MSG_SIZE : 0;
    serial.write(&buf[start], (buf_pos - start) * MSG_SIZE);
    state |= STATE_ACK_AWAIT;
    state &= ~STATE_IDLE_SEND;
    last_write_activity = time;
    buf_pos_sent = buf_pos;
  }
  void process_timeouts(uint16_t time) {
    if (serial.available() != last_serial_available_value) {
      last_serial_available_value = serial.available();
      if (state & STATE_IDLE_RECEIVE)
        receive_time_diff = 0;
      state &= ~STATE_IDLE_RECEIVE;
      last_read_activity = time;
    } else if ((state & STATE_IDLE_RECEIVE) == 0 &&
               time - last_read_activity >= TIME_IDLE_RECEIVE) {
      if ((state & (STATE_IDLE_SEND | STATE_IDLE_RECEIVE)) == STATE_IDLE_SEND)
        last_write_activity = time; // for ACK timeout
      state &= ~STATE_MSG_DISCARD;
      state |= STATE_IDLE_RECEIVE;
      if (received) { // Acknoledge received messages immediately
        uint8_t buf[3];
        msg(MSG_ACK, received, &buf[0]);
        serial.write(buf, MSG_SIZE);
        state &= ~STATE_IDLE_SEND;
        last_write_activity = time;
        received = 0;
      }
    }
    if (serial.availableForWrite() != serial_write_available_max) {
      state &= ~STATE_IDLE_SEND;
      last_write_activity = time;
    } else if ((state & STATE_IDLE_SEND) == 0 &&
               time - last_write_activity >= TIME_IDLE_SEND) {
      state |= STATE_IDLE_SEND;
      if (state & STATE_IDLE_RECEIVE)
        last_write_activity = time; // for ACK timeout
    }
    if ((state & (STATE_IDLE_SEND | STATE_IDLE_RECEIVE | STATE_ACK_AWAIT)) ==
        (STATE_IDLE_SEND | STATE_IDLE_RECEIVE | STATE_ACK_AWAIT) &&
        time - last_write_activity >= ACK_TIMEOUT) {
      state &= ~STATE_ACK_AWAIT;
    }
  }
  void receive_messages(uint16_t time) {
    uint8_t lbuf[MSG_SIZE];
    uint16_t body;
    while (serial.available() >= 3) {
      serial.readBytes(lbuf, MSG_SIZE);
      uint8_t crc = lbuf[0] >> 4;
      lbuf[0] &= 0x0F;
      if (crc != crc4(lbuf, MSG_SIZE)) {
        state |= STATE_MSG_DISCARD;
        msgs_discard();
        return 0; // MSG_NONE
      }
      memcpy(&body, lbuf + 1, sizeof(body));
      switch (lbuf[0]) {
        case MSG_ACK:
          if ((state & (STATE_ACK_AWAIT | STATE_IDLE_SEND)) == (STATE_ACK_AWAIT | STATE_IDLE_SEND)) {
            state &= ~(STATE_MARK_SENT_TIME | STATE_ACK_AWAIT);
            mark_time_diff_from_localtime = 0;
            uint8_t dec = body - ((buf_pos - buf_pos_sent) % msgs_buffered);
            buf_msgs = (buf_msgs > dec) ? buf_msgs : 0;
          }
          continue;
        case MSG_TIME:
          receive_time_diff = time - body - RECEIVE_LATENCY_MS;
          continue;
        default:
          ++received; // Ack and settime are ignored in msg count
          msg_callback(lbuf[0], body, receive_time_diff);
          continue;
      }
    }
  }
public:
  uint16_t receive_time_diff = 0;
  HardwareSerial &serial;
  void (*msg_callback)(uint8_t, uint16_t, uint16_t);
  MessageTransceiver(HardwareSerial &serial, void (*parse_messages)(uint8_t, uint16_t, uint16_t))
  : serial{serial}, msg_callback{parse_messages} {serial_write_available_max = serial.availableForWrite();}
  
  void msg_queue(uint8_t type, uint16_t body) {
    if (buf_msgs == 0)
      buf_pos = 0;
    msg(type, body, &buf[buf_pos * MSG_SIZE]);
    buf_pos = (buf_pos + 1) % msgs_buffered;
    buf_msgs = MIN(buf_msgs + 1, msgs_buffered);
    if (buf_pos == buf_pos_sent) // No messages to resend
      state &= ~STATE_ACK_AWAIT;
  }

  void msg_process() {
    uint16_t time = millis();
    process_timeouts(time);
    if ((state & (STATE_ACK_AWAIT | STATE_IDLE_SEND)) == (STATE_IDLE_SEND))
      send_buffers(time);
    if (state & STATE_MSG_DISCARD || 
      (last_serial_available_value > 0 && last_serial_available_value < MSG_SIZE && state & STATE_IDLE_RECEIVE)) {
      msgs_discard();
      return 0; // MSG_NONE
    }
    receive_messages(time);
  }
  
  void msg_mark_time(uint16_t mark_diff_from_localtime = 0) {
    state |= STATE_MARK_SENT_TIME;
    mark_time_diff_from_localtime = mark_diff_from_localtime;
  }
};

#endif // MESSAGES_HPP