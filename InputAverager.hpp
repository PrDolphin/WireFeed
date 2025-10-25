#ifndef INPUT_AVERAGER_H
#define INPUT_AVERAGER_H
#define ANALOG_INPUT_AVERAGE_FACTOR 10
#include <inttypes.h>
class InputAverager {
private:
  uint16_t avg[ANALOG_INPUT_AVERAGE_FACTOR];
  uint8_t pin;
public:
  inline InputAverager(uint8_t pin): pin(pin), avg{} {};
  uint16_t get();
};
#endif // INPUT_AVERAGER_H