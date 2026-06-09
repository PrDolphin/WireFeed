/* Vibe-coded using ChatGPT 5 mini */

#include "at25xxx_dma.h"
#include <string.h>
#if USE_FREERTOS > 0
#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <task.h>
#endif

/* Opcodes */
#define AT25_OP_READ      0x03
#define AT25_OP_WRITE     0x02
#define AT25_OP_RDSR      0x05
#define AT25_OP_WREN      0x06

/* Internal helpers */
static inline void cs_low(AT25DMA_HandleTypeDef *dev){ HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET); }
static inline void cs_high(AT25DMA_HandleTypeDef *dev){ HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET); }

void AT25DMA_CS_Low(AT25DMA_HandleTypeDef *dev){ cs_low(dev); }
void AT25DMA_CS_High(AT25DMA_HandleTypeDef *dev){ cs_high(dev); }

AT25_Status AT25DMA_Init(AT25DMA_HandleTypeDef *dev){
  if (!dev || !dev->hspi || !dev->cs_port)
    return AT25_ERR_PARAM;
  if (dev->spi_timeout_ms == 0)
    dev->spi_timeout_ms = 10;
  if (dev->write_timeout_ms == 0)
    dev->write_timeout_ms = 10;
#if USE_FREERTOS > 0
#else
  dev->dma_processing = 0;
#endif
  cs_high(dev);
  return AT25_OK;
}

#if USE_FREERTOS > 0
static void wait_prepare(AT25DMA_HandleTypeDef *dev) {
  dev->current_task = osThreadGetId();
}
static AT25_Status wait_complete(AT25DMA_HandleTypeDef *dev, uint32_t timeout_ms) {
  return ulTaskNotifyTake(pdTRUE, timeout_ms) ? AT25_OK : AT25_ERR_TIMEOUT;
}
#else
static void wait_prepare(AT25DMA_HandleTypeDef *dev) {
  if (dev->dma_processing)
    return AT25_ERR_BUSY;
  dev->dma_processing = 1;
}
/* Poll helpers for DMA completion (simple busy wait with HAL_GetTick timeout) */
static AT25_Status wait_complete(AT25DMA_HandleTypeDef *dev, uint32_t timeout_ms) {
  uint32_t t0 = HAL_GetTick();
  while(dev->dma_processing){
    if((HAL_GetTick() - t0) > timeout_ms) return AT25_ERR_TIMEOUT;
  }
  dev->dma_processing = 0;
  return AT25_OK;
}

#endif

/* ReadStatus: send RDSR and read one byte (use TxRx DMA) */
AT25_Status AT25DMA_ReadStatus(AT25DMA_HandleTypeDef *dev, uint8_t *status){
  if (!dev || !status)
    return AT25_ERR_PARAM;
  uint8_t tx[2] = { AT25_OP_RDSR, 0xFF };
  uint8_t rx[2] = {0,0};
  cs_low(dev);
  wait_prepare(dev);
  if (HAL_SPI_TransmitReceive_DMA(dev->hspi, tx, rx, 2) != HAL_OK) {
    cs_high(dev);
    return AT25_ERR_HAL;
  }
  if (wait_complete(dev, dev->spi_timeout_ms) != AT25_OK) {
    cs_high(dev);
    return AT25_ERR_TIMEOUT;
  }
  cs_high(dev);
  *status = rx[1];
  return AT25_OK;
}

/* Write enable: simple TX of single byte */
AT25_Status AT25DMA_WriteEnable(AT25DMA_HandleTypeDef *dev){
  if (!dev)
    return AT25_ERR_PARAM;
  uint8_t cmd = AT25_OP_WREN;
  cs_low(dev);
  wait_prepare(dev);
  if (HAL_SPI_Transmit_DMA(dev->hspi, &cmd, 1) != HAL_OK) {
    cs_high(dev);
    return AT25_ERR_HAL;
  }
  if (wait_complete(dev, dev->spi_timeout_ms) != AT25_OK) {
    cs_high(dev);
    return AT25_ERR_TIMEOUT;
  }
  cs_high(dev);
  return AT25_OK;
}

/* Wait write complete by polling RDSR WIP bit */
AT25_Status AT25DMA_WaitWriteComplete(AT25DMA_HandleTypeDef *dev, uint32_t timeout_ms){
  uint32_t t0 = HAL_GetTick();
  uint8_t st;
  for (;;) {
    if (AT25DMA_ReadStatus(dev, &st) != AT25_OK) return AT25_ERR_HAL;
    if ((st & 0x01) == 0) return AT25_OK;
    if ((HAL_GetTick() - t0) > timeout_ms) return AT25_ERR_TIMEOUT;
#if USE_FREERTOS > 0
    osDelay(1);
#else
    HAL_Delay(1);
#endif
  }
}

/* Read: send opcode+addr (TX) then read len bytes via RX DMA (separate transmit of header + receive) */
AT25_Status AT25DMA_Read(AT25DMA_HandleTypeDef *dev, uint16_t addr, uint8_t *buf, size_t len) {
  if(!dev || !buf) return AT25_ERR_PARAM;
  if(len == 0) return AT25_OK;
  addr &= AT25_ADDR_MASK;

  uint8_t hdr[2];
  hdr[0] = AT25_OP_READ;
  hdr[1] = (uint8_t)(addr & 0xFF);

  cs_low(dev);
  /* transmit header using DMA (blocking wait) */
  wait_prepare(dev);
  if (HAL_SPI_Transmit_DMA(dev->hspi, hdr, 2) != HAL_OK) {
    cs_high(dev); return AT25_ERR_HAL;
  }
  if (wait_complete(dev, dev->spi_timeout_ms) != AT25_OK) {
    cs_high(dev); return AT25_ERR_TIMEOUT;
  }

  /* now receive data: HAL_SPI_Receive_DMA will clock out 0xFF automatically via SPI peripheral */
  wait_prepare(dev);
  if (HAL_SPI_Receive_DMA(dev->hspi, buf, (uint16_t)len) != HAL_OK) {
    cs_high(dev); return AT25_ERR_HAL;
  }
  if (wait_complete(dev, dev->spi_timeout_ms + len) != AT25_OK) {
    cs_high(dev); return AT25_ERR_TIMEOUT;
  }
  cs_high(dev);
  return AT25_OK;
}

/* Write: handles page boundaries. For each page-chunk: WREN, send WRITE+addr+data using Tx DMA (single buffer assembled) */
AT25_Status AT25DMA_Write (AT25DMA_HandleTypeDef *dev, uint16_t addr, const uint8_t *buf, size_t len) {
  if(!dev || !buf) return AT25_ERR_PARAM;
  if(len == 0) return AT25_OK;

  size_t remaining = len;
  uint16_t current_addr = addr & AT25_ADDR_MASK;
  const uint8_t *p = buf;

  while(remaining){
    uint16_t page_offset = current_addr % AT25_PAGE_SIZE;
    uint16_t space = AT25_PAGE_SIZE - page_offset;
    uint16_t chunk = (remaining < space) ? remaining : space;

    /* enable write */
    if(AT25DMA_WriteEnable(dev) != AT25_OK) return AT25_ERR_HAL;

    /* prepare tx buffer: opcode + addr + chunk data */
    uint8_t tmp[AT25_PAGE_SIZE + 2];
    tmp[0] = AT25_OP_WRITE;
    tmp[1] = (uint8_t)(current_addr & 0xFF);
    memcpy(&tmp[2], p, chunk);

    cs_low(dev);
    wait_prepare(dev);
    if(HAL_SPI_Transmit_DMA(dev->hspi, tmp, (uint16_t)(2 + chunk)) != HAL_OK){
      cs_high(dev); return AT25_ERR_HAL;
    }
    if(wait_complete(dev, dev->spi_timeout_ms + chunk) != AT25_OK) {
      cs_high(dev); return AT25_ERR_TIMEOUT;
    }
    cs_high(dev);

    /* wait internal write cycle complete (max ~5ms per datasheet) */
    if(AT25DMA_WaitWriteComplete(dev, dev->write_timeout_ms) != AT25_OK) return AT25_ERR_TIMEOUT;

    /* advance */
    p += chunk;
    remaining -= chunk;
    current_addr = (current_addr + chunk) & AT25_ADDR_MASK;
  }
  return AT25_OK;
}

AT25_Status AT25DMA_Update(AT25DMA_HandleTypeDef *dev, uint16_t addr, const uint8_t *buf, size_t len) {
  if(!dev || !buf) return AT25_ERR_PARAM;
  if(len == 0) return AT25_OK;

  uint8_t page_buf[AT25_PAGE_SIZE];
  uint16_t current_addr = addr & AT25_ADDR_MASK;
  size_t remaining = len;
  const uint8_t *p = buf;

  while(remaining){
    uint16_t page_offset = current_addr % AT25_PAGE_SIZE;
    uint16_t space = AT25_PAGE_SIZE - page_offset;
    uint16_t chunk = (remaining < space) ? remaining : space;

    uint16_t page_start = (current_addr - page_offset) & AT25_ADDR_MASK;
    AT25_Status st = AT25DMA_Read(dev, page_start, page_buf, AT25_PAGE_SIZE);
    if(st != AT25_OK) return st;

    /* scan page region [page_offset, page_offset+chunk) for differing segments */
    uint16_t i = 0;
    while(i < chunk){
      if(page_buf[page_offset + i] == p[i]){ i++; continue; }

      /* start of differing segment */
      uint16_t seg_start = i;
      uint16_t seg_len = 1;
      i++;
      while(i < chunk && page_buf[page_offset + i] != p[i]){
        seg_len++; i++;
      }

      /* prepare data to write: source is p + seg_start, length seg_len
               write address = page_start + page_offset + seg_start */
      uint16_t write_addr = (page_start + page_offset + seg_start) & AT25_ADDR_MASK;
      st = AT25DMA_Write(dev, write_addr, &p[seg_start], seg_len);
      if(st != AT25_OK) return st;
      /* after successful write, update page_buf to reflect new content (optional, but keeps consistency if further segments overlap) */
      memcpy(&page_buf[page_offset + seg_start], &p[seg_start], seg_len);
    }

    /* advance pointers */
    p += chunk;
    remaining -= chunk;
    current_addr = (current_addr + chunk) & AT25_ADDR_MASK;
  }

  return AT25_OK;
}


/* --- Callback linkage ---
   The user application should call HAL SPI callbacks and route them to these functions
   from their global HAL callbacks, e.g.:

   void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) { AT25DMA_SPI_TxCpltCallback(hspi); }
   void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) { AT25DMA_SPI_RxCpltCallback(hspi); }
   void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) { AT25DMA_SPI_TxRxCpltCallback(hspi); }

   The functions below check hspi pointer match and set flags.
   To support multiple devices, compare hspi to each device instance or adapt callbacks to call device-specific handlers.
*/

/* Callbacks called by user HAL wrappers */
void AT25DMA_SPI_TxCpltCallback(AT25DMA_HandleTypeDef *dev){
#if USE_FREERTOS == 0
  if(dev)
    dev->dma_processing = 0;
#else
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR(dev->current_task, 1, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
#endif
}

void AT25DMA_SPI_RxCpltCallback(AT25DMA_HandleTypeDef *dev){
#if USE_FREERTOS == 0
  if(dev)
    dev->dma_processing = 0;
#else
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR(dev->current_task, 1, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
#endif
}

void AT25DMA_SPI_TxRxCpltCallback(AT25DMA_HandleTypeDef *dev){
#if USE_FREERTOS == 0
  if(dev)
    dev->dma_processing = 0;
#else
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR(dev->current_task, 1, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
#endif
}

/* End of file */