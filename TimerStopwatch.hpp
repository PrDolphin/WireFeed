#include <Arduino.h>

#define TIMERSTOPWATCH_POLL_INTERVAL 50
#define TIMERSTOPWATCH_TICKS_PER_SEC 1000
#define TIMERSTOPWATCH_TICK 0x1
#define TIMERSTOPWATCH_MODE_CHANGED 0x2
#define TIMERSTOPWATCH_RESET 0x3

template <typename T>
class TimerStopwatch {
private:
  uint16_t nextupdate = 0;
  uint16_t timernexttick = TIMERSTOPWATCH_TICKS_PER_SEC;
  uint8_t startstoppin;
  uint8_t resetpin;
  uint8_t modswitchpin;
  uint8_t flags = 0;
  enum FLAGS {
    TICKING=0x1,
    MODE_TIMER=0x2,
    STARTSTOPPRESSED=0x4
  };
public:
  T seconds = 0;
  T startseconds = 60;
  TimerStopwatch(uint8_t startstoppin, uint8_t resetpin, uint8_t modswitchpin)
  : startstoppin{startstoppin}, resetpin{resetpin}, modswitchpin{modswitchpin} {};
  uint8_t tick(uint16_t time) {
    if (time - nextupdate >= 0x8000)
      return 0;
    nextupdate += TIMERSTOPWATCH_POLL_INTERVAL;
    if (!digitalRead(startstoppin)) {
      if (flags & TimerStopwatch<T>::STARTSTOPPRESSED)
        return 0;
      flags ^= TimerStopwatch<T>::TICKING;
      flags |= TimerStopwatch<T>::STARTSTOPPRESSED;
      timernexttick += (flags & TimerStopwatch<T>::TICKING) ? time : -time; // Keep milliseconds intact until next activation
      if (seconds == 0 && (flags & TimerStopwatch<T>::MODE_TIMER))
        seconds = startseconds;
      return TIMERSTOPWATCH_MODE_CHANGED;
    }
    flags &= ~TimerStopwatch<T>::STARTSTOPPRESSED;
    if (!digitalRead(resetpin)) {
      seconds = 0;
      timernexttick = TIMERSTOPWATCH_TICKS_PER_SEC;
      flags &= ~(TimerStopwatch<T>::TICKING);
      return TIMERSTOPWATCH_RESET;
    }
    if ((flags & TimerStopwatch<T>::TICKING) == 0) {
      if (seconds != 0 ||
          !digitalRead(modswitchpin) == ((flags & TimerStopwatch<T>::MODE_TIMER) > 0))
        return 0;
      flags ^= TimerStopwatch<T>::MODE_TIMER;
      return TIMERSTOPWATCH_MODE_CHANGED;
    }
    if (time - timernexttick >= 0x8000)
      return 0;
    timernexttick += TIMERSTOPWATCH_TICKS_PER_SEC;
    if ((flags & TimerStopwatch<T>::MODE_TIMER) == 0) {
      ++seconds;
      return TIMERSTOPWATCH_TICK;
    }
    seconds = (seconds <= 1) ? startseconds : seconds - 1;
    return TIMERSTOPWATCH_TICK;
  }

  bool isTimer() {
    return (flags & TimerStopwatch<T>::MODE_TIMER) > 0;
  }

  bool isTicking() {
    return (flags & TimerStopwatch<T>::TICKING) > 0;
  }

  uint8_t tick() {
    return tick(millis());
  }
};