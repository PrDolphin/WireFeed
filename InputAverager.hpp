#ifndef INPUT_AVERAGER_H
#define INPUT_AVERAGER_H
#define ANALOG_INPUT_AVERAGE_HISTORY 20
#define ANALOG_INPUT_AVERAGE_NEW 10
#define ANALOG_INPUT_MAX_DELTA 25
#define ANALOG_INPUT_MIN_REPEATS 25
#include <inttypes.h>
class InputAverager {
private:
  uint16_t avg[ANALOG_INPUT_AVERAGE_HISTORY - 1];
  uint16_t range;
  uint16_t last;
  uint8_t pin;
  uint8_t repeat_counter;
public:
  inline InputAverager(uint8_t pin, uint16_t map_max_range=1024): pin(pin), range{map_max_range}, avg{}, last{}, repeat_counter{} {};
  uint16_t get();
};
#endif // INPUT_AVERAGER_H