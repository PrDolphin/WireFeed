#ifndef AT25XXX_DMA_H
#define AT25XXX_DMA_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stddef.h>
#include "at25xxx_dma_conf.h"

typedef enum {
  AT25_OK = 0,
  AT25_ERR_TIMEOUT,
  AT25_ERR_PARAM,
  AT25_ERR_HAL,
  AT25_ERR_BUSY,
} AT25_Status;

typedef struct {
  SPI_HandleTypeDef *hspi;
  GPIO_TypeDef *cs_port;
  uint32_t spi_timeout_ms;    /* used for blocking waiting on flags */
  uint32_t write_timeout_ms;  /* timeout for internal write cycle waits */
  uint16_t cs_pin;
#if USE_FREERTOS > 0
  void *current_task;
#else
  volatile uint8_t dma_processing;
#endif
} AT25DMA_HandleTypeDef;

/* API */
AT25_Status AT25DMA_Init(AT25DMA_HandleTypeDef *dev);
void AT25DMA_CS_Low(AT25DMA_HandleTypeDef *dev);
void AT25DMA_CS_High(AT25DMA_HandleTypeDef *dev);

AT25_Status AT25DMA_ReadStatus(AT25DMA_HandleTypeDef *dev, uint8_t *status);
AT25_Status AT25DMA_WriteEnable(AT25DMA_HandleTypeDef *dev);
AT25_Status AT25DMA_WaitWriteComplete(AT25DMA_HandleTypeDef *dev, uint32_t timeout_ms);

AT25_Status AT25DMA_Read(AT25DMA_HandleTypeDef *dev, uint16_t addr, uint8_t *buf, size_t len);
AT25_Status AT25DMA_Write(AT25DMA_HandleTypeDef *dev, uint16_t addr, const uint8_t *buf, size_t len);
AT25_Status AT25DMA_Update(AT25DMA_HandleTypeDef *dev, uint16_t addr, const uint8_t *buf, size_t len);

/* To be called from HAL_SPI_TxCpltCallback callbacks in user code */
void AT25DMA_SPI_TxCpltCallback(AT25DMA_HandleTypeDef *dev);
/* To be called from HAL_SPI_RxCpltCallback callbacks in user code */
void AT25DMA_SPI_RxCpltCallback(AT25DMA_HandleTypeDef *dev);
/* To be called from HAL_SPI_TxRxCpltCallback callbacks in user code */
void AT25DMA_SPI_TxRxCpltCallback(AT25DMA_HandleTypeDef *dev);

#endif
