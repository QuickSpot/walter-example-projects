/**
 * @file network_dce.c
 * @author Robbe Beernaert <robbe@dptechnics.com>
 * @author Thibo Verheyde <thibo@dptechnics.com>
 * @date 19 March 2026
 * @copyright DPTechnics bv
 * @brief Sequans GM02SP modem DCE wrapper
 *
 * @section LICENSE
 *
 * Copyright (C) 2026, DPTechnics bv
 * All rights reserved.
 *
  * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * @section DESCRIPTION
 *
 * Wraps the esp-modem DCE for the Sequans GM02SP LTE modem connected to the
 * ESP32-S3 over UART with hardware flow control. Exposes a simplified API for
 * initialising the modem, performing a hard reset via the open-drain RESET
 * line, entering and leaving PPP data mode, checking AT command sync, and
 * querying signal quality (RSSI).
 */

#include <string.h>
#include "driver/gpio.h"
#include "esp_netif.h"
#include "esp_modem_api.h"
#include "esp_modem_c_api_types.h"

#define EXAMPLE_MODEM_TX_PIN CONFIG_EXAMPLE_MODEM_TX_PIN
#define EXAMPLE_MODEM_RX_PIN CONFIG_EXAMPLE_MODEM_RX_PIN
#define EXAMPLE_MODEM_CTS_PIN CONFIG_EXAMPLE_MODEM_CTS_PIN
#define EXAMPLE_MODEM_RTS_PIN CONFIG_EXAMPLE_MODEM_RTS_PIN
#define EXAMPLE_MODEM_RESET_PIN CONFIG_EXAMPLE_MODEM_RESET_PIN

#define EXAMPLE_MODEM_PPP_APN CONFIG_EXAMPLE_MODEM_PPP_APN

static esp_modem_dce_t *dce = NULL;

void modem_reset();


esp_err_t modem_init_network(esp_netif_t *netif)
{
    // setup the DCE
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);

    dte_config.uart_config.tx_io_num = EXAMPLE_MODEM_TX_PIN;
    dte_config.uart_config.rx_io_num = EXAMPLE_MODEM_RX_PIN;
    dte_config.uart_config.cts_io_num = EXAMPLE_MODEM_CTS_PIN;
    dte_config.uart_config.rts_io_num = EXAMPLE_MODEM_RTS_PIN;
    dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_HW;

    dce = esp_modem_new_dev(ESP_MODEM_DCE_SQNGM02S, &dte_config, &dce_config, netif);
    if (!dce) {
        return ESP_FAIL;
    }

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT_OD;
    io_conf.pin_bit_mask = (1ULL << EXAMPLE_MODEM_RESET_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_hold_dis((gpio_num_t)EXAMPLE_MODEM_RESET_PIN);

    modem_reset();

#ifdef CONFIG_EXAMPLE_NEED_SIM_PIN
    // configure the PIN
    bool pin_ok = false;
    if (esp_modem_read_pin(dce, &pin_ok) == ESP_OK && pin_ok == false) {
        if (esp_modem_set_pin(dce, CONFIG_EXAMPLE_SIM_PIN) == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            abort();
        }
    }
#endif // CONFIG_EXAMPLE_NEED_SIM_PIN

    esp_modem_PdpContext_t pdp_context = {1, "IP", EXAMPLE_MODEM_PPP_APN};
    esp_modem_sqn_gm02s_connect(dce, &pdp_context);
    
    return ESP_OK;
}

void modem_deinit_network(void)
{
    if (dce) {
        esp_modem_destroy(dce);
        dce = NULL;
    }
}

bool modem_start_network()
{
    return esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA) == ESP_OK;
}

bool modem_stop_network()
{
    return esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
}

bool modem_check_sync()
{
    return esp_modem_sync(dce) == ESP_OK;
}

void modem_reset()
{
    gpio_set_level((gpio_num_t)EXAMPLE_MODEM_RESET_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)EXAMPLE_MODEM_RESET_PIN, 1);

    esp_modem_at_raw(dce, "", NULL, "+SYSSTART", "ERROR", 15000);
    vTaskDelay(pdMS_TO_TICKS(2000));
}

bool modem_check_signal()
{
    int rssi, ber;
    if (esp_modem_get_signal_quality(dce, &rssi, &ber) == ESP_OK) {
        return rssi != 99 && rssi > 5;
    }
    return false;
}
