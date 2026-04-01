/**
 * @file main.cpp
 * @author Daan Pape <daan@dptechnics.com>, Arnoud Devoogdt <arnoud@dptechnics.com>, Robbe Beernaert
 * @date 21 Aug 2025
 * @copyright DPTechnics bv
 * @brief Walter unified comms library examples
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
 * This program sets up the Unified comms and connects to a cloud MQTT service.
 */

#include "mqtt_client.h"
#include <bsp/walter.hpp>
#include <netif/ppp/ppp.h>

static bool mqtt_connected = false;

static const char* TAG = "UC_MQTT";

static esp_mqtt_client_handle_t client = nullptr;

static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id,
                               void* event_data)
{
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

  switch(event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT connected");
    {
      const char* topic = "walter-mqtt-test-topic";
      const char* message = "Hello from Walter device!";
      int msg_id = esp_mqtt_client_publish(client, topic, message, 0, 1, 0);
      ESP_LOGI(TAG, "Published message with id: %d", msg_id);
    }
    mqtt_connected = true;
    break;

  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT disconnected");
    mqtt_connected = false;
    break;

  case MQTT_EVENT_PUBLISHED:
    ESP_LOGI(TAG, "MQTT message published, msg_id=%d", event->msg_id);
    break;

  case MQTT_EVENT_ERROR:
    ESP_LOGE(TAG, "MQTT event error");
    break;

  default:
    break;
  }
}

extern "C" void app_main(void)
{
  ESP_LOGI(TAG, "Unified Comms MQTT example V1.0.0");

  CELL_DRV(uc.GM02S)->config("CELL-APN", 6);
  WIFI_DRV(uc.ESP_WIFI)->configStation("WIFI-SSID", "WIFI-PASSWORD", 5);

  if(uc.controller.start()) {
    ESP_LOGI(TAG, "Succesfully started unified comms");
  } else {
    ESP_LOGE(TAG, "Could not start unified comms / connect a driver");
  }

  vTaskDelay(pdMS_TO_TICKS(5000));

  esp_mqtt_client_config_t mqtt_cfg = {};
  mqtt_cfg.broker.address.uri = "mqtt://broker.hivemq.com:1883";

  client = esp_mqtt_client_init(&mqtt_cfg);

  // Register event handler for all MQTT events
  esp_mqtt_client_register_event(client, esp_mqtt_event_id_t::MQTT_EVENT_ANY, mqtt_event_handler,
                                 nullptr);

  esp_mqtt_client_start(client);

  while(true) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    if(mqtt_connected) {
      const char* topic = "walter-mqtt-test-topic";
      const char* message = "Hello again from Walter!";
      int msg_id = esp_mqtt_client_publish(client, topic, message, 0, 1, 0);
      ESP_LOGI(TAG, "Published in loop, msg_id=%d", msg_id);
    } else {
      ESP_LOGW(TAG, "MQTT not connected, skipping publish");
    }
  }
}