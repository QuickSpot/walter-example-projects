#pragma once

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arducam/ArducamCamera.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void spiCsOutputMode(int pin);
uint8_t spiReadWriteByte(ArducamCamera* cam, uint8_t data);  // forward declare ArducamCamera or include it

#ifdef __cplusplus
}
#endif
