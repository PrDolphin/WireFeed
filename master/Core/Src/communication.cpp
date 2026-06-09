#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"

extern "C" {
#include "Modbus.h"
}

#include "TimerStopwatch.hpp"
#include "communication_constants.h"
#include "communication.h"
#include "acceleration.h"

#define POLL_INTERVAL pdMS_TO_TICKS(100)
#define WRITE_MINDELAY 10000

#define VAR_SPEED 0
#define VAR_COEF 1
#define VAR_TIMERSTARTTIME 2

#define EEPROM_START_ADDRESS 0x0

#define SLAVE_ADDRESS 0x21
#define SLAVE_COIL_MANAGED 0x1
#define SLAVE_COIL_MOTOR_ENABLED 0x2
#define MANAGED_MODE_EXPIRES 10000

static uint16_t motors_target_speed = 1;
static int16_t coef_offset = 0;
static uint16_t timer_starttime = 1;
static uint16_t last_var_update_time = 0;
static uint8_t dirty_vars = 0;
static bool motor_enabled = false;
static uint8_t managed_mode = false;
uint32_t managed_mode_checktime = 0;
static uint16_t hregs[2] = {0};
static uint16_t coils = 0;

TimerStopwatch<uint16_t> timerstopwatch{};

modbusHandler_t mb_master{
  .uModbusType = MB_MASTER,
  .port = &huart2,
  .u8id = 0,
  .EN_Port = USART2_DIR_GPIO_Port,
  .EN_Pin = USART2_DIR_Pin,
  .u16regs = nullptr,
  .u16timeOut = TIMEOUT_MODBUS,
  .u16regsize = 0,
  .xTypeHW = USART_HW_DMA,
};

modbusHandler_t mb_slave{
  .uModbusType = MB_SLAVE,
  .port = &huart1,
  .u8id = SLAVE_ADDRESS,
  .EN_Port = USART1_DIR_GPIO_Port,
  .EN_Pin = USART1_DIR_Pin,
  .u16regs = NULL,
  .u16timeOut = TIMEOUT_MODBUS,
  .u16regsize = 0,
  .u16coils = &coils,
  .u16coilsNregs = 2,
  .u16holdingRegs = hregs,
  .u16holdingRegsNregs = sizeof(hregs)/sizeof(hregs[0]),
  .xTypeHW = USART_HW_DMA,
};

#define MB_ERR(x) \
if ((status = x) != OP_OK_QUERY) \
  goto MB_ERROR

#define BV(x) (1 << x)
#define ISFLAG(x, f) ((x & (1 << f)) == (1 << f))
#define NOTFLAG(x, f) ((x & (1 << f)) == 0)

#define millis() (xTaskGetTickCount() * (1000/configTICK_RATE_HZ))

extern "C" HAL_StatusTypeDef rtc_reset_subseconds() {
  __HAL_LOCK(&hrtc);
  __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
  RTC_EnterInitMode(&hrtc);
  hrtc.Instance->SSR = 0;
  RTC_ExitInitMode(&hrtc);
  __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);
  __HAL_UNLOCK(&hrtc);
  return HAL_OK;
}

extern "C" void commInit(void) {
  ModbusInit(&mb_master);
  ModbusInit(&mb_slave);
  ModbusStart(&mb_master);
  ModbusStart(&mb_slave);
}

static uint32_t processController1() {
  uint32_t status;
  uint16_t coils;
  MB_ERR(ModbusQueryV2(&mb_master, modbus_t{
    .u8id = MODBUS_ADDRESS1,
    .u8fct = MB_FC_READ_COILS,
    .u16RegAdd = 0,
    .u16CoilsNo = MODBUS_COILS1,
    .u16reg = &coils
  }));
  // Initialize new controller
  if (NOTFLAG(coils, MODBUS_COIL_CONTROLLER_INIT)) {
    uint16_t registers[MODBUS_HREGS1] = {timerstopwatch.seconds, motors_target_speed};
    MB_ERR(ModbusQueryV2(&mb_master, modbus_t{
      .u8id = MODBUS_ADDRESS1,
      .u8fct = MB_FC_WRITE_MULTIPLE_REGISTERS,
      .u16RegAdd = 0,
      .u16CoilsNo = MODBUS_HREGS1,
      .u16reg = registers
    }));
    coils = BV(MODBUS_COIL_CONTROLLER_INIT)
            | (motor_enabled << MODBUS_COIL_MOTOR_ENABLED)
            | (timerstopwatch.ticking() << MODBUS_COIL_TIMER_ENABLED)
            | (0 << MODBUS_COIL_TIMER_RESET);
    MB_ERR(ModbusQueryV2(&mb_master, modbus_t{
      .u8id = MODBUS_ADDRESS1,
      .u8fct = MB_FC_WRITE_MULTIPLE_COILS,
      .u16RegAdd = 0,
      .u16CoilsNo = MODBUS_COILS1,
      .u16reg = &coils
    }));
    return 0;
  }
  if (ISFLAG(coils, MODBUS_COIL_TIMER_ENABLED)) {

    if (!timerstopwatch.ticking()) {
      rtc_reset_subseconds();
      timerstopwatch.start(RTC->TR & 0xF);
      (void)RTC->DR; // Read is needed to unblock RTC registers
    }
  } else {
    timerstopwatch.stop();
  }
  if (ISFLAG(coils, MODBUS_COIL_TIMER_RESET)) {
    timerstopwatch.reset();
    uint16_t val = 0;
    MB_ERR(ModbusQueryV2(&mb_master, modbus_t{
      .u8id = MODBUS_ADDRESS1,
      .u8fct = MB_FC_WRITE_COIL,
      .u16RegAdd = MODBUS_COIL_TIMER_RESET,
      .u16CoilsNo = 1,
      .u16reg = &val
    }));
  }
  if (managed_mode) {
    MB_ERR(ModbusQueryV2(&mb_master, modbus_t{
      .u8id = MODBUS_ADDRESS1,
      .u8fct = MB_FC_WRITE_REGISTER,
      .u16RegAdd = MODBUS_HREG_SPEED,
      .u16CoilsNo = 1,
      .u16reg = &motors_target_speed
    }));
    return 0;
  }
  uint16_t new_target_speed;
  MB_ERR(ModbusQueryV2(&mb_master, modbus_t{
    .u8id = MODBUS_ADDRESS1,
    .u8fct = MB_FC_READ_REGISTERS,
    .u16RegAdd = MODBUS_HREG_SPEED,
    .u16CoilsNo = 1,
    .u16reg = &new_target_speed
  }));
  if (new_target_speed != motors_target_speed) {
    motors_target_speed = new_target_speed;
    last_var_update_time = millis();
    dirty_vars = 1;
  }
  motor_enabled = ISFLAG(coils, MODBUS_COIL_MOTOR_ENABLED);
  setSpeed((motor_enabled) ? motors_target_speed : 0);
  return 0;
MB_ERROR:
  return status;
}

static uint32_t processController2() {
  uint32_t status;
  uint16_t coils = 0;
  MB_ERR(ModbusQueryV2(&mb_master, modbus_t{
    .u8id = MODBUS_ADDRESS2,
    .u8fct = MB_FC_READ_COILS,
    .u16RegAdd = 0,
    .u16CoilsNo = MODBUS_COILS2,
    .u16reg = &coils
  }));
  // Initialize new controller
  if (NOTFLAG(coils, MODBUS_COIL_CONTROLLER_INIT)) {
    uint16_t registers[MODBUS_HREGS2] = {timerstopwatch.seconds, (uint16_t)coef_offset, timer_starttime};
    MB_ERR(ModbusQueryV2(&mb_master, modbus_t{
      .u8id = MODBUS_ADDRESS2,
      .u8fct = MB_FC_WRITE_MULTIPLE_REGISTERS,
      .u16RegAdd = 0,
      .u16CoilsNo = MODBUS_HREGS2,
      .u16reg = registers
    }));
    coils = BV(MODBUS_COIL_CONTROLLER_INIT)
            | (timerstopwatch.timer() << MODBUS_COIL_TIMER_STOPWATCH_SELECT);
    MB_ERR(ModbusQueryV2(&mb_master, modbus_t{
      .u8id = MODBUS_ADDRESS2,
      .u8fct = MB_FC_WRITE_MULTIPLE_COILS,
      .u16RegAdd = 0,
      .u16CoilsNo = MODBUS_COILS2,
      .u16reg = &coils
    }));
    return 0;
  }
  uint16_t registers[MODBUS_HREGS2];
  if (managed_mode) {
    MB_ERR(ModbusQueryV2(&mb_master, modbus_t{
      .u8id = MODBUS_ADDRESS1,
      .u8fct = MB_FC_WRITE_REGISTER,
      .u16RegAdd = MODBUS_HREG_COEF,
      .u16CoilsNo = 1,
      .u16reg = (uint16_t*)&coef_offset
    }));
    MB_ERR(ModbusQueryV2(&mb_master, modbus_t{
      .u8id = MODBUS_ADDRESS2,
      .u8fct = MB_FC_READ_REGISTERS,
      .u16RegAdd = MODBUS_HREG_TIMER_STARTTIME,
      .u16CoilsNo = 1,
      .u16reg = registers + MODBUS_HREG_TIMER_STARTTIME
    }));
    registers[MODBUS_HREG_COEF] = coef_offset;
  } else {
    MB_ERR(ModbusQueryV2(&mb_master, modbus_t{
      .u8id = MODBUS_ADDRESS2,
      .u8fct = MB_FC_READ_REGISTERS,
      .u16RegAdd = 0,
      .u16CoilsNo = MODBUS_HREGS2,
      .u16reg = registers
    }));
  }
  if ((int16_t)registers[MODBUS_HREG_COEF] != coef_offset) {
    coef_offset = registers[MODBUS_HREG_COEF];
    setCoefOff2(coef_offset);
    last_var_update_time = millis();
    dirty_vars = 1;
  }
  bool ticking;
  ticking = timerstopwatch.ticking();
  if (ISFLAG(coils, MODBUS_COIL_TIMER_STOPWATCH_SELECT)) {
    timerstopwatch.setmode_timer(registers[MODBUS_HREG_TIMER_STARTTIME]);
  } else {
    timerstopwatch.setmode_stopwatch();
  }
  if (ticking != timerstopwatch.ticking()) {
    uint16_t val = 0;
    for (unsigned i = 0; i < 3 &&
      ModbusQueryV2(&mb_master, modbus_t{
        .u8id = MODBUS_ADDRESS1,
        .u8fct = MB_FC_WRITE_COIL,
        .u16RegAdd = MODBUS_COIL_TIMER_ENABLED,
        .u16CoilsNo = 1,
        .u16reg = &val
      }) != OP_OK_QUERY; ++i);
    // Try several times at least
  }
  if (timer_starttime != registers[MODBUS_HREG_TIMER_STARTTIME]) {
    timer_starttime = registers[MODBUS_HREG_TIMER_STARTTIME];
    last_var_update_time = millis();
    dirty_vars = 1;
  }
  return 0;
MB_ERROR:
  return status;
}

extern "C" void sendTime() {
  ModbusQueryV2(&mb_master, modbus_t{
                           .u8id = MODBUS_ADDRESS1,//ADDRESS_BROADCAST,
                           .u8fct = MB_FC_WRITE_REGISTER,
                           .u16RegAdd = MODBUS_HREG_TIME,
                           .u16CoilsNo = 1,
                           .u16reg = &timerstopwatch.seconds
  });
  ModbusQueryV2(&mb_master, modbus_t{
                           .u8id = MODBUS_ADDRESS2,//ADDRESS_BROADCAST,
                           .u8fct = MB_FC_WRITE_REGISTER,
                           .u16RegAdd = MODBUS_HREG_TIME,
                           .u16CoilsNo = 1,
                           .u16reg = &timerstopwatch.seconds
                         });
}

extern "C" void loadVars(void) {
  uint16_t buf[3] = {0};
  while (AT25DMA_Read(&heeprom, EEPROM_START_ADDRESS, (uint8_t*)buf, sizeof(buf)) != AT25_OK);
  motors_target_speed = buf[VAR_SPEED];
  coef_offset = (int16_t)buf[VAR_COEF];
  timer_starttime = buf[VAR_TIMERSTARTTIME];
  dirty_vars = 0;
}

extern "C" void saveVars(void) {
  if (dirty_vars && ((uint16_t)millis()) - last_var_update_time >= WRITE_MINDELAY) {
    uint16_t buf[] = {
      [VAR_SPEED] = motors_target_speed,
      [VAR_COEF] = (uint16_t)coef_offset,
      [VAR_TIMERSTARTTIME] = timer_starttime};
    if (AT25DMA_Update(&heeprom, EEPROM_START_ADDRESS, (uint8_t*)buf, sizeof(buf)) == AT25_OK)
      dirty_vars = 0;
  }
}

extern "C" void StartCommTask(void *arg) {
  vTaskDelay(pdMS_TO_TICKS(100));
  TickType_t xLastWakeTime = xTaskGetTickCount();
  loadVars();
  for (;;) {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    xSemaphoreTake((QueueHandle_t)mb_slave.ModBusSphrHandle, portMAX_DELAY);
    if (coils & SLAVE_COIL_MANAGED) {
      coils &= ~SLAVE_COIL_MANAGED;
      managed_mode = 1;
      managed_mode_checktime = millis();
    } else if (managed_mode && millis() - managed_mode_checktime >= MANAGED_MODE_EXPIRES) {
      managed_mode = 2;
      loadVars();
    }
    if (managed_mode) {
      motor_enabled = coils & SLAVE_COIL_MOTOR_ENABLED;
      motors_target_speed = hregs[0];
      coef_offset = hregs[1];
      setSpeed((motor_enabled) ? motors_target_speed : 0);
    }
    xSemaphoreGive((QueueHandle_t)mb_slave.ModBusSphrHandle);
    processController1();
    processController2();
    uint32_t tr = RTC->TR;
    uint32_t dr = RTC->DR;
    (void)dr;
    timerstopwatch.tick(tr & 0xF);
    sendTime();
    if (!managed_mode) {
      saveVars();
    } else if (managed_mode == 2)
      managed_mode = 0;
    vTaskDelayUntil(&xLastWakeTime, POLL_INTERVAL);
  }
}