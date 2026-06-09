#include <stdint.h>

#define ONE_SECOND_TIME 1u
#define TIME_OVERFLOW_VAL 10

template <typename T>
class TimerStopwatch {
private:
  uint8_t state = 0;
  uint16_t lastsecond_time = 0;
  enum FLAGS {
    TICKING=0x1,
    MODE_TIMER=0x2,
    RESET=0x4,
  };
public:
  T seconds = 0;
  T startseconds = 0;
  void start(uint16_t time) {
    state |= TimerStopwatch::TICKING;
    lastsecond_time = time;
    state &= ~TimerStopwatch::RESET;
  }
  void stop() {
    state &= ~TimerStopwatch::TICKING;
  }
  void stop(T time) {
    state &= ~TimerStopwatch::TICKING;
    seconds = time;
  }
  void reset() {
    state &= ~TimerStopwatch::TICKING;
    state |= TimerStopwatch::RESET;
    seconds = startseconds;
  }
  void setmode_timer(T startseconds) {
    this->startseconds = startseconds;
    if ((state & TimerStopwatch::MODE_TIMER) == 0) {
      reset();
    } else if (state & TimerStopwatch::RESET) {
      this->seconds = startseconds;
    }
    state |= TimerStopwatch::MODE_TIMER;
  }
  void setmode_stopwatch() {
    if ((state & TimerStopwatch::MODE_TIMER) == 0)
      return;
    state &= ~(TimerStopwatch::MODE_TIMER | TimerStopwatch::TICKING);
    startseconds = 0;
    reset();
  }

  bool tick(uint16_t time) {
    if ((state & TimerStopwatch::TICKING) == 0 || (time - lastsecond_time < ONE_SECOND_TIME))
      return false;
    if (state & TimerStopwatch::MODE_TIMER)
      if (seconds <= 1)
        seconds = startseconds;
      else
        seconds -= 1;
    else
      seconds += 1;
    lastsecond_time = (lastsecond_time >= TIME_OVERFLOW_VAL - 1) ? 0 : lastsecond_time + ONE_SECOND_TIME;
    return true;
  }
  bool ticking() {
    return (state & TimerStopwatch::TICKING) == TimerStopwatch::TICKING;
  }
  bool timer() {
    return (state & TimerStopwatch::MODE_TIMER) == TimerStopwatch::MODE_TIMER;
  }
};