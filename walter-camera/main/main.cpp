/**
 * @file walter-as-linux-modem.ino
 * @author Daan Pape <daan@dptechnics.com>
 * @date 23 Apr 2025
 * @copyright DPTechnics bv
 * @brief Walter based LTE-M camera
 *
 * @section LICENSE
 *
 * Copyright (C) 2025, DPTechnics bv
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * 
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 * 
 *   3. Neither the name of DPTechnics bv nor the names of its contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 * 
 *   4. This software, with or without modification, must only be used with a
 *      Walter board from DPTechnics bv.
 * 
 *   5. Any software provided in binary form under this license must not be
 *      reverse engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY DPTECHNICS BV “AS IS” AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL DPTECHNICS BV OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @section DESCRIPTION
 * 
 * This file contains firmware for Wlater that connects an Arducam B0434 to the
 * Walter module over SPI. Walter uploads a picture of the camera over LTE-M to
 * the QuickSpot demo server every x-amount of time. The interval can be tweaked
 * from minutes to hours.
 */

#include <stdio.h>
#include <string.h>
#include <WalterModem.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "esp_system.h"
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include <driver/temp_sensor.h>
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#include "Arducam_Mega.h"

// Camera and LED pinout
#define PIN_SPI_SCLK 10
#define PIN_SPI_MISO 9
#define PIN_SPI_MOSI 8
#define PIN_SPI_CS 18
#define PIN_LED_PWR 17
#define PIN_LED_GND 16
#define WALTER_PIN_PWR_3V3_EN 0

// QuickSpot demo portal configuration
#define QUICKSPOT_SERVER_ADDRESS "64.225.64.140"
#define QUICKSPOT_SERVER_PORT 1999
#define QUICKSPOT_IMG_PKT_SIZE 1024
#define QUICKSPOT_METADATA_PKT_SIZE 29

// Quickspot simple image chunking protocol defines
#define MAC_ADDRESS_LENGTH 6
#define IMAGE_ID_BUFSIZE 1
#define IMAGE_TOTAL_BUFSIZE 4
#define IMAGE_CHUNK_BUFSIZE 2
#define IMAGE_HEADER_SIZE (MAC_ADDRESS_LENGTH + IMAGE_ID_BUFSIZE + IMAGE_TOTAL_BUFSIZE + IMAGE_CHUNK_BUFSIZE)
#define UDP_DATA_HEADER_SIZE (MAC_ADDRESS_LENGTH + IMAGE_ID_BUFSIZE + IMAGE_CHUNK_BUFSIZE)
#define UDP_PAYLOAD_MAX (QUICKSPOT_IMG_PKT_SIZE - UDP_DATA_HEADER_SIZE)

/**
 * @brief Tag used for the esp-log library.
 */
static const char* TAG = "[WALTER CAM]";

/**
 * @brief The serial peripheral to use to communicate with the modem.
 */
static const uint8_t modem_serial = 1;

/**
 * @brief Buffer for the metadata packets.
 */
static uint8_t quickspot_metadata_buffer[QUICKSPOT_METADATA_PKT_SIZE];

/**
 * @brief Buffer for the image packets.
 */
static uint8_t quickspot_img_buffer[QUICKSPOT_IMG_PKT_SIZE];

/**
 * @brief The current image id which is sent to the server.
 */
static uint8_t image_id_counter = 0;

/**
 * @brief The SPI peripheral used to communicate with the camera module.
 */
static spi_device_handle_t spi_handle;

/**
 * @brief The camera module handle.
 */
static Arducam_Mega camera;

/**
 * @brief Buffer for the Walter modem response.
 */
WalterModemRsp rsp = {};

/**
 * @brief This function checks if we are connected to the lte network
 *
 * @return True when connected, false otherwise
 */
bool is_lte_connected() {
  WalterModemNetworkRegState regState = WalterModem::getNetworkRegState();
  return regState == WALTER_MODEM_NETWORK_REG_REGISTERED_HOME || regState == WALTER_MODEM_NETWORK_REG_REGISTERED_ROAMING;
}

/**
 * @brief Initialize the SPI bus.
 * 
 * @return ESP_OK on success or an error code otherwise.
 */
static esp_err_t init_spi_bus() {
  spi_bus_config_t buscfg = {
    .mosi_io_num = PIN_SPI_MOSI,
    .miso_io_num = PIN_SPI_MISO,
    .sclk_io_num = PIN_SPI_SCLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4096,
  };
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

  spi_device_interface_config_t devcfg = {
    .command_bits = 0,
    .address_bits = 0,
    .dummy_bits = 0,
    .mode = 0,                          // SPI mode 0
    .clock_speed_hz = 10 * 1000 * 1000, // 10 MHz
    .spics_io_num = PIN_SPI_CS,         // CS pin
    .flags = 0,
    .queue_size = 1,
    .pre_cb = NULL,
    .post_cb = NULL,
  };

  ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle));
  vTaskDelay(pdMS_TO_TICKS(1000));
  camera.create(PIN_SPI_CS, spi_handle);

  return ESP_OK;
}

/**
 * @brief Set the up modem and configure it to connect.
 * 
 * @param rsp Pointer to the response buffer.
 */
void setup_modem(WalterModemRsp* rsp) {
  static const char* _logtag = "[MODEM]";
  WalterModemRAT rat = WALTER_MODEM_RAT_UNKNOWN;

  if (WalterModem::getRAT(rsp)) {
    rat = rsp->data.rat;
  } else {
    ESP_LOGE(_logtag, "Could not retrieve radio access technology");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return;
  }

  if (rat != WALTER_MODEM_RAT_LTEM) {
    rat = rat == WALTER_MODEM_RAT_LTEM ? WALTER_MODEM_RAT_NBIOT : WALTER_MODEM_RAT_LTEM;

    if (!WalterModem::setRAT(rat)) {
      ESP_LOGE(_logtag, "Could not switch radio access technology");
      vTaskDelay(pdMS_TO_TICKS(1000));
      esp_restart();
      return;
    }
    ESP_LOGI(_logtag, "Switched radio access technology");
  }

  /* Set operational state to MINIMUM */
  if (WalterModem::setOpState(WALTER_MODEM_OPSTATE_MINIMUM)) {
    ESP_LOGI(_logtag, "Successfully set operational state to MINIMUM\n");
  } else {
    ESP_LOGW(_logtag, "Could not set operational state to MINIMUM\n");
    return;
  }

  if (!WalterModem::configGNSS()) {
    ESP_LOGE(_logtag, "Could not configure the GNSS subsystem\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return;
  }

  /* Create PDP context */
  if (WalterModem::createPDPContext("soracom.io")) {
    ESP_LOGI(_logtag, "Created PDP context\n");
  } else {
    ESP_LOGW(_logtag, "Could not create PDP context\n");
    return;
  }

  /* Authenticate the PDP context */
  if (WalterModem::authenticatePDPContext()) {
    ESP_LOGI(_logtag, "Authenticated the PDP context\n");
  } else {
    ESP_LOGW(_logtag, "Could not authenticate the PDP context\n");
    return;
  }

  if (WalterModem::configCEREGReports(
        WALTER_MODEM_CEREG_REPORTS_ENABLED_UE_PSM_WITH_LOCATION_EMM_CAUSE)) {
    ESP_LOGI(
      _logtag,
      "Configured CEREG to receive PSM result allocated by the network \n");
  } else {
    ESP_LOGW(_logtag,
             "Could not configure CEREG to receive PSM result "
             "allocated by the network\n");
  }

  /* Enable /disable eDRX */
  if (WalterModem::configEDRX(WALTER_MODEM_EDRX_DISABLE)) {
    ESP_LOGI(_logtag, "Configured eDRX\n");
  } else {
    ESP_LOGW(_logtag, "Could not configure eDRX\n");
  }

  if (WalterModem::setOpState(WALTER_MODEM_OPSTATE_FULL)) {
    ESP_LOGI(_logtag, "Successfully set operational state to FULL\n");
  } else {
    ESP_LOGW(_logtag, "Could not set operational state to FULL\n");
    return;
  }

  /* Set the network operator selection to automatic */
  if (!WalterModem::setNetworkSelectionMode(
        WALTER_MODEM_NETWORK_SEL_MODE_AUTOMATIC)) {
    ESP_LOGW(_logtag,
             "Could not set the network selection mode to automatic\n");
    return;
  }

  ESP_LOGI(_logtag, "Connecting to the %s network\n",
           rat == WALTER_MODEM_RAT_LTEM ? "LTE-M" : "NB-IoT");

  int count = 0;
  WalterModemNetworkRegState regState = WalterModem::getNetworkRegState();
  while (!(regState == WALTER_MODEM_NETWORK_REG_REGISTERED_HOME || regState == WALTER_MODEM_NETWORK_REG_REGISTERED_ROAMING)) {
    if (count >= 1800) {
      ESP_LOGW(_logtag,
               "Could not connect to the network, going to to try other RAT");
      break;
    }

    count += 1;
    vTaskDelay(pdMS_TO_TICKS(1000));
    regState = WalterModem::getNetworkRegState();
  }

  if (!(regState == WALTER_MODEM_NETWORK_REG_REGISTERED_HOME || regState == WALTER_MODEM_NETWORK_REG_REGISTERED_ROAMING)) {
    /* Set the operational state to minimum */
    if (!WalterModem::setOpState(WALTER_MODEM_OPSTATE_MINIMUM)) {
      ESP_LOGW(_logtag, "Could not set operational state to MINIMUM\n");
      return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    rat = rat == WALTER_MODEM_RAT_LTEM ? WALTER_MODEM_RAT_NBIOT : WALTER_MODEM_RAT_LTEM;

    if (!WalterModem::setRAT(rat)) {
      ESP_LOGE(_logtag, "Could not switch radio access technology");
      vTaskDelay(pdMS_TO_TICKS(1000));
      esp_restart();
      return;
    }

    ESP_LOGI(_logtag, "Switched modem radio technology");

    /* Set the operational state to full */
    if (!WalterModem::setOpState(WALTER_MODEM_OPSTATE_FULL)) {
      ESP_LOGW(_logtag, "Could not set operational state to FULL\n");
      return;
    }

    /* Set the network operator selection to automatic */
    if (!WalterModem::setNetworkSelectionMode(
          WALTER_MODEM_NETWORK_SEL_MODE_AUTOMATIC)) {
      ESP_LOGW(_logtag,
               "Could not set the network selection mode to automatic\n");
      return;
    }

    ESP_LOGI(_logtag, "Connecting to %s network\n",
             rat == WALTER_MODEM_RAT_LTEM ? "LTE-M" : "NB-IoT");

    /* Wait for the network to become available (max 30 minutes) */
    count = 0;
    regState = WalterModem::getNetworkRegState();
    while (!(regState == WALTER_MODEM_NETWORK_REG_REGISTERED_HOME || regState == WALTER_MODEM_NETWORK_REG_REGISTERED_ROAMING)) {
      if (count >= 1800) {
        ESP_LOGE(_logtag, "Could not connect to the network, going to reboot");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
        return;
      }

      count += 1;
      vTaskDelay(pdMS_TO_TICKS(1000));
      regState = WalterModem::getNetworkRegState();
    }
  }

  ESP_LOGI(_logtag, "Connected to the network\n");
}

/**
 * @brief Open an UDP socket to the QuicSpot server.
 */
static void open_udp_socket() {
  WalterModem::createSocket();
  WalterModem::configSocket();
  WalterModem::connectSocket(QUICKSPOT_SERVER_ADDRESS, QUICKSPOT_SERVER_PORT, QUICKSPOT_SERVER_PORT);
}

/**
 * @brief Transmit an UDP datagram to the QuickSpot server.
 * 
 * @param data The databuffer to transmit.
 * @param length The length of the databuffer to transmit.
 */
static void transmit_udp_data(uint8_t* data, uint16_t length) {
  ESP_LOGI(TAG, "Transmitting %d bytes", length);
  if (!WalterModem::socketSend(data, length)) {
    ESP_LOGE(TAG, "UDP transmission failed");
    esp_restart();
  }
}

/**
 * @brief Close the UDP socket.
 */
static void close_udp_socket(void) {
  WalterModem::closeSocket();
}

/**
 * @brief Read Walter's internal temperature sensor.
 * 
 * @return The temperature in degrees Celsius.
 */
float read_walter_temp() {
  float result = -9999;
  temp_sensor_config_t tsens = { .dac_offset = TSENS_DAC_L2, .clk_div = 6 };
  temp_sensor_set_config(tsens);
  temp_sensor_start();
  temp_sensor_read_celsius(&result);
  temp_sensor_stop();

  return result;
}

/**
 * @brief Read the sensors and transmit the data.
 */
void read_and_transmit_metadata() {
  ESP_LOGI(TAG, "Reading and transmitting metadata");

  /* Construct a Walter Feels packet */
  esp_read_mac(quickspot_metadata_buffer, ESP_MAC_WIFI_STA);

  ESP_LOGI(TAG, "Walter's MAC is: %02X:%02X:%02X:%02X:%02X:%02X\n",
           quickspot_metadata_buffer[0],
           quickspot_metadata_buffer[1],
           quickspot_metadata_buffer[2],
           quickspot_metadata_buffer[3],
           quickspot_metadata_buffer[4],
           quickspot_metadata_buffer[5]);

  /* Read the temperature of Walter */
  float temp = read_walter_temp();
  ESP_LOGI(TAG, "The temperature of Walter is %.02f degrees Celsius", temp);

  /* Construct the metadata packet buffer */
  uint16_t rawTemp = (temp + 50) * 100;
  quickspot_metadata_buffer[6] = 0x02;
  quickspot_metadata_buffer[7] = rawTemp >> 8;
  quickspot_metadata_buffer[8] = rawTemp & 0xFF;
  quickspot_metadata_buffer[9] = 0;
  memset(quickspot_metadata_buffer + 10, 0, 4);
  memset(quickspot_metadata_buffer + 14, 0, 4);

  if (!WalterModem::getCellInformation(WALTER_MODEM_SQNMONI_REPORTS_SERVING_CELL, &rsp)) {
    ESP_LOGE(TAG, "Could not request cell information");
  } else {

    ESP_LOGI(TAG, "Connected on band %u using operator %s (%u%02u) and cell ID %lu",
             rsp.data.cellInformation.band, rsp.data.cellInformation.netName,
             rsp.data.cellInformation.cc, rsp.data.cellInformation.nc,
             rsp.data.cellInformation.cid);

    ESP_LOGI(TAG, "Signal strength: RSRP: %.2f, RSRQ: %.2f",
             rsp.data.cellInformation.rsrp, rsp.data.cellInformation.rsrq);
  }

  quickspot_metadata_buffer[18] = rsp.data.cellInformation.cc >> 8;
  quickspot_metadata_buffer[19] = rsp.data.cellInformation.cc & 0xFF;
  quickspot_metadata_buffer[20] = rsp.data.cellInformation.nc >> 8;
  quickspot_metadata_buffer[21] = rsp.data.cellInformation.nc & 0xFF;
  quickspot_metadata_buffer[22] = rsp.data.cellInformation.tac >> 8;
  quickspot_metadata_buffer[23] = rsp.data.cellInformation.tac & 0xFF;
  quickspot_metadata_buffer[24] = (rsp.data.cellInformation.cid >> 24) & 0xFF;
  quickspot_metadata_buffer[25] = (rsp.data.cellInformation.cid >> 16) & 0xFF;
  quickspot_metadata_buffer[26] = (rsp.data.cellInformation.cid >> 8) & 0xFF;
  quickspot_metadata_buffer[27] = rsp.data.cellInformation.cid & 0xFF;
  quickspot_metadata_buffer[28] = (uint8_t)(rsp.data.cellInformation.rsrp * -1);

  open_udp_socket();
  transmit_udp_data(quickspot_metadata_buffer, QUICKSPOT_METADATA_PKT_SIZE);
  close_udp_socket();
}

/**
 * @brief Capture a camera image and transmit it to the QuickSpot server.
 */
static void capture_and_transmit_image() {
  camera.takePicture(CAM_IMAGE_MODE_VGA, CAM_IMAGE_PIX_FMT_JPG);
  uint32_t total_size = camera.getReceivedLength();
  if (total_size == 0) {
    ESP_LOGE(TAG, "Empty image capture");
    return;
  }

  ESP_LOGI(TAG, "Captured image size: %ld bytes", total_size);

  open_udp_socket();
  vTaskDelay(pdMS_TO_TICKS(500));

  uint8_t mac[MAC_ADDRESS_LENGTH];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);

  uint16_t total_chunks = (total_size + UDP_PAYLOAD_MAX - 1) / UDP_PAYLOAD_MAX;
  uint8_t header[IMAGE_HEADER_SIZE];

  memcpy(header, mac, MAC_ADDRESS_LENGTH);
  header[MAC_ADDRESS_LENGTH] = image_id_counter++;
  header[MAC_ADDRESS_LENGTH + 1] = (total_size >> 24) & 0xFF;
  header[MAC_ADDRESS_LENGTH + 2] = (total_size >> 16) & 0xFF;
  header[MAC_ADDRESS_LENGTH + 3] = (total_size >> 8) & 0xFF;
  header[MAC_ADDRESS_LENGTH + 4] = total_size & 0xFF;
  header[MAC_ADDRESS_LENGTH + 5] = (total_chunks >> 8) & 0xFF;
  header[MAC_ADDRESS_LENGTH + 6] = total_chunks & 0xFF;

  transmit_udp_data(header, IMAGE_HEADER_SIZE);
  vTaskDelay(pdMS_TO_TICKS(500));

  uint32_t bytes_left = total_size;
  for (uint16_t chunk_index = 0; chunk_index < total_chunks; ++chunk_index) {
    memcpy(quickspot_img_buffer, mac, MAC_ADDRESS_LENGTH);
    quickspot_img_buffer[MAC_ADDRESS_LENGTH] = header[MAC_ADDRESS_LENGTH]; // image_id
    quickspot_img_buffer[MAC_ADDRESS_LENGTH + 1] = (chunk_index >> 8) & 0xFF;
    quickspot_img_buffer[MAC_ADDRESS_LENGTH + 2] = chunk_index & 0xFF;

    uint16_t this_length = (bytes_left < UDP_PAYLOAD_MAX) ? bytes_left : UDP_PAYLOAD_MAX;
    for (uint16_t i = 0; i < this_length; ++i) {
      quickspot_img_buffer[UDP_DATA_HEADER_SIZE + i] = camera.readByte();
    }
    bytes_left -= this_length;

    ESP_LOGI(TAG, "Chunk %d/%d, size: %d bytes", chunk_index + 1, total_chunks, this_length);

    if (this_length < UDP_PAYLOAD_MAX) {
      memset(&quickspot_img_buffer[UDP_DATA_HEADER_SIZE + this_length], 0, UDP_PAYLOAD_MAX - this_length);
      this_length = UDP_PAYLOAD_MAX;
    }

    transmit_udp_data(quickspot_img_buffer, UDP_DATA_HEADER_SIZE + this_length);
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  ESP_LOGI(TAG, "Finished transmitting chunked image");
  close_udp_socket();
}

/**
 * @brief Main application entrypoint.
 */
extern "C" void app_main(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // Enable 3V3
  gpio_hold_dis((gpio_num_t)WALTER_PIN_PWR_3V3_EN);
  gpio_set_pull_mode((gpio_num_t)WALTER_PIN_PWR_3V3_EN, GPIO_FLOATING);
  gpio_set_level((gpio_num_t)WALTER_PIN_PWR_3V3_EN, 0);

  // Enable camera activity LED
  gpio_reset_pin((gpio_num_t)PIN_LED_PWR);
  gpio_set_direction((gpio_num_t)PIN_LED_PWR, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)PIN_LED_PWR, 1);

  gpio_reset_pin((gpio_num_t)PIN_LED_GND);
  gpio_set_direction((gpio_num_t)PIN_LED_GND, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)PIN_LED_GND, 0);

  /* Modem initialization */
  if (WalterModem::begin(modem_serial)) {
    ESP_LOGI(TAG, "Modem initialization OK");
  } else {
    ESP_LOGE(TAG, "Modem initialization ERROR");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return;
  }

  setup_modem(&rsp);

  // Initialize SPI bus and camera
  vTaskDelay(pdMS_TO_TICKS(3000));
  ESP_ERROR_CHECK(init_spi_bus());
  ESP_LOGI(TAG, "SPI bus initialized");
  vTaskDelay(pdMS_TO_TICKS(5000));
  camera.begin();

  // Send an initial verification image
  vTaskDelay(pdMS_TO_TICKS(3000));
  capture_and_transmit_image();
  vTaskDelay(pdMS_TO_TICKS(5000));

  camera.setAutoExposure(1);
  camera.setAutoISOSensitive(1);

  while (true) {
    if (!is_lte_connected()) {
      ESP_LOGE(TAG, "No longer connected to LTE network, restarting...");
      vTaskDelay(pdMS_TO_TICKS(10000));
      esp_restart();
      continue;
    }

    capture_and_transmit_image();
    vTaskDelay(pdMS_TO_TICKS(15000));
    read_and_transmit_metadata();

    // Configure delay here, by default 1 hour
    vTaskDelay(pdMS_TO_TICKS(60 * 60 * 1000));
  }
}
