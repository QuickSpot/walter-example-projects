/**
 * @file main.c
 * @author Daan Pape (daan@dptechnics.com)
 * @brief This code connects to the BlueCherry platform.
 * @version 0.1
 * @date 2025-07-14
 * @copyright Copyright (c) 2025 DPTechnics BV
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/lgpl-3.0.html>.
 */

#include <esp_chip_info.h>
#include <bsp/walter.hpp>
#include <esp_system.h>
#include "bluecherry.h"
#include <sdkconfig.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <inttypes.h>
#include <esp_log.h>
#include <string.h>
#include <stdio.h>

#define BLUECHERRY_DEVICE_TYPE "walter01"

/**
 * @brief The logging tag for this BlueCherry module.
 */
static const char* TAG = "UC_BC";

/**
 * @brief The device certificate from the symbol section of the firmware.
 */
extern const char devcert[] asm("_binary_devcert_pem_start");

/**
 * @brief The device key from the symbol section of the firmware.
 */
extern const char devkey[] asm("_binary_devkey_pem_start");

/**
 * @brief Handle an incoming MQTT message.
 *
 * This function handles an incoming MQTT message.
 *
 * @param topic The topic as the topic index.
 * @param len The length of the incoming data.
 * @param data The incoming data buffer.
 * @param args A NULL pointer.
 */
static void bluecherry_msg_handler(uint8_t topic, uint16_t len, const uint8_t* data, void* args)
{
  ESP_LOGI(TAG, "Received MQTT message of length %d on topic %02X: %.*s", len, topic, len, data);
}

esp_err_t nvs_write_str(const char* key, const char* value)
{
  nvs_handle_t handle;
  ESP_ERROR_CHECK(nvs_open("bcztp_store", NVS_READWRITE, &handle));
  ESP_ERROR_CHECK(nvs_set_str(handle, key, value));
  ESP_ERROR_CHECK(nvs_commit(handle));
  nvs_close(handle);
  return ESP_OK;
}

esp_err_t nvs_read_str(const char* key, char* buf, size_t len)
{
  nvs_handle_t handle;
  esp_err_t err = nvs_open("bcztp_store", NVS_READONLY, &handle);
  if(err != ESP_OK)
    return err;

  err = nvs_get_str(handle, key, buf, &len);
  nvs_close(handle);
  return err;
}

static const char* bluecherry_ztp_bio_handler(bool read, bool secure, void* args)
{
  static char devcert[4096];
  static char devkey[4096];

  const char* keyname = secure ? "bcztp_key" : "bcztp_cert";

  if(read) {
    esp_err_t err =
        nvs_read_str(keyname, secure ? devkey : devcert, secure ? sizeof(devkey) : sizeof(devcert));
    if(err != ESP_OK) {
      ESP_LOGW(TAG, "No %s found in NVS (err=0x%x)", keyname, err);
      return NULL;
    }
    return secure ? devkey : devcert;
  } else {
    const char* data = (const char*) args;

    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open("bcztp_store", NVS_READWRITE, &handle));

    if(data == NULL) {
      ESP_LOGW(TAG, "Erasing %s from NVS", keyname);
      nvs_erase_key(handle, keyname);
      nvs_commit(handle);
      nvs_close(handle);
      return NULL;
    }

    ESP_ERROR_CHECK(nvs_set_str(handle, keyname, data));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);

    ESP_LOGI(TAG, "Stored %s in NVS", keyname);
    return data;
  }
}

/**
 * @brief The main application entrypoint.
 */
extern "C" void app_main(void)
{
  ESP_LOGI(TAG, "Unified Comms BlueCherry example V1.0.0");

  CELL_DRV(uc.GM02S)->config("CELL-APN", 6);
  WIFI_DRV(uc.ESP_WIFI)->configStation("WIFI-SSID", "WIFI-PASSWORD", 5);

  if(uc.controller.start()) {
    ESP_LOGI(TAG, "Succesfully started unified comms");
  } else {
    ESP_LOGE(TAG, "Could not start unified comms / connect a driver");
  }

  /* Initialize bluecherry with pre-provisioned keys */
  // while (!bluecherry_init(devcert, devkey, bluecherry_msg_handler, NULL, true, 30)) {
  //   ESP_LOGI(TAG, "Waiting for Initial bluecherry connection...");
  //   vTaskDelay(pdMS_TO_TICKS(5000));
  // }

  /* Initialize bluecherry with zero-touch provisioning */
  while (bluecherry_init_ztp(bluecherry_ztp_bio_handler, NULL, BLUECHERRY_DEVICE_TYPE,
                                      bluecherry_msg_handler, NULL, true, 30) != ESP_OK) {
    ESP_LOGI(TAG, "Waiting for Initial bluecherry connection...");
    vTaskDelay(pdMS_TO_TICKS(5000));
  }

  while(true) {
    ESP_LOGI(TAG, "Publishing message");
    bluecherry_publish(0x84, strlen("Test message") + 1, (const uint8_t*) "Test message");
    vTaskDelay(pdMS_TO_TICKS(5000));
    if(esp_task_wdt_status(NULL) == ESP_OK) {
      esp_task_wdt_reset();
    }
  }
}