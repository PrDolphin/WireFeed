#ifndef NUMBER_BUTTONS_H
#define NUMBER_BUTTONS_H

#ifndef NUMBER_POLL_INTERVAL
#define NUMBER_POLL_INTERVAL 100
#endif // NUMBER_POLL_INTERVAL
#ifndef NUMBER_SCROLL_START
#define NUMBER_SCROLL_START NUMBER_POLL_INTERVAL * 5
#endif // NUMBER_SCROLL_START
#ifndef NUMBER_WRITE_DELAY
#define NUMBER_WRITE_DELAY NUMBER_POLL_INTERVAL * 10
#endif // NUMBER_WRITE_INTERVAL
#ifndef NUMBER_EEPROM_OFFSET
#define NUMBER_EEPROM_OFFSET 0
#endif // NUMBER_EEPROM_OFFSET

#define NUMBER_CHANGED 1
#define NUMBER_WRITTEN 2

#include <Arduino.h>
#include <inttypes.h>
#include <EEPROM.h>

template<typename _Tp>
struct is_signed
{
  static bool const value = _Tp(-1) < _Tp(0);
};

template <typename T>
class NumberButtons {
private:
  enum FLAGS {
    SCROLLING = 0x1,
    WRITE = 0x2
  };
  uint8_t eeprom;
  uint8_t flags;
  uint16_t check_time;
  uint16_t scroll_write_time;
public:
  uint8_t pins[2];
  T number;
  T limit;
  
  NumberButtons (uint8_t add_pin, uint8_t sub_pin, uint8_t eeprom_address, T sym_limit)
  : pins{add_pin, sub_pin}, eeprom{eeprom_address}, limit{sym_limit}
  , flags{0}, check_time{0}, number{0}
  {
    for (uint8_t i = 0; i < sizeof(T); ++i) {
      number |= (T)EEPROM.read(NUMBER_EEPROM_OFFSET + eeprom) << (i * 8);
    }
    number = (number > limit) ? limit
      : (is_signed<T>::value && number < -limit) ? -limit :  number;
  };
  
  uint8_t tick (uint16_t time) {
    if (time - check_time >= 0x8000) {
      return 0;
    }
    check_time += NUMBER_POLL_INTERVAL;
    bool add = !digitalRead(pins[0]);
    if (add == !digitalRead(pins[1])) { // If both or none buttons are pressed
      if (flags & NumberButtons<T>::SCROLLING) {
        scroll_write_time = time + NUMBER_WRITE_DELAY;
        flags &= ~NumberButtons<T>::SCROLLING;
      }
      if ((flags & NumberButtons<T>::WRITE) == 0 || time - scroll_write_time >= 0x8000)
        return 0;
      
      for (uint8_t i = 0; i < sizeof(T); ++i) {
        EEPROM.update(NUMBER_EEPROM_OFFSET + eeprom, number >> (i * 8));
      }
      flags &= ~NumberButtons<T>::WRITE;
      return NUMBER_WRITTEN;
    }
    if ((flags & NumberButtons<T>::SCROLLING) && time - scroll_write_time >= 0x8000)
      return 0;
    number += (add) ? 1 : -1;
    if (number > limit)
      number = (is_signed<T>::value) ? -limit : 0;
    if (is_signed<T>::value && number < -limit)
      number = limit;
    if ((flags & NumberButtons<T>::SCROLLING) == 0) {
      scroll_write_time = time + NUMBER_SCROLL_START;
    } else {
      scroll_write_time = time;
    }
    flags |= NumberButtons<T>::SCROLLING | NumberButtons<T>::WRITE;
    return NUMBER_CHANGED;
  }
  inline void tick () {
    tick(millis());
  };
};
#endif // NUMBER_BUTTONS_H