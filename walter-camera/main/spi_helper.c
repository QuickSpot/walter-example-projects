#include "spi_helper.h"
#include "Arducam/ArducamCamera.h"

void spiCsOutputMode(int pin) {
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

uint8_t spiReadWriteByte(ArducamCamera* cam, uint8_t data) {
    uint8_t rx = 0;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
        .rx_buffer = &rx
    };
    esp_err_t ret = spi_device_transmit(cam->spi_handle, &t);
    assert(ret == ESP_OK);
    return rx;
}
