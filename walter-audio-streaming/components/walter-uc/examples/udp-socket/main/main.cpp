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
 * This program sets up the Unified comms and connects with a UDP socket to walterdemo website
 */

#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <bsp/walter.hpp>
#include <esp_mac.h>

static const char* TAG = "UC_UDP";

extern "C" void app_main(void)
{
  ESP_LOGI(TAG, "Unified Comms UDP Socket example V1.0.0");

  CELL_DRV(uc.GM02S)->config("CELL-APN", 6);
  WIFI_DRV(uc.ESP_WIFI)->configStation("WIFI-SSID", "WIFI-PASSWORD", 5);

  if(uc.controller.start()) {
    ESP_LOGI(TAG, "Succesfully started unified comms");
  } else {
    ESP_LOGE(TAG, "Could not start unified comms / connect a driver");
  }

  vTaskDelay(pdMS_TO_TICKS(5000));

  const char* host = "walterdemo.quickspot.io";
  const char* port = "1999";

  struct addrinfo hints;
  struct addrinfo* res = nullptr;
  int sock = -1;

  // Keep retrying DNS resolution until it succeeds
  while(true) {
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int err = getaddrinfo(host, port, &hints, &res);
    if(err != 0 || res == nullptr) {
      ESP_LOGE(TAG, "DNS lookup failed: %d, retrying...", err);
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    // Try to create socket
    sock = socket(res->ai_family, res->ai_socktype, 0);
    if(sock < 0) {
      ESP_LOGE(TAG, "Failed to create socket, retrying...");
      freeaddrinfo(res);
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    ESP_LOGI(TAG, "Socket created successfully");
    break; // success → exit retry loop
  }

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);

  uint16_t counter = 0;

  // Main loop: keep sending packets forever
  while(true) {
    uint8_t buffer[8];
    memcpy(buffer, mac, 6);
    buffer[6] = (counter >> 8) & 0xFF; // MSB
    buffer[7] = counter & 0xFF;        // LSB
    counter++;

    int sent = sendto(sock, buffer, sizeof(buffer), 0, res->ai_addr, res->ai_addrlen);
    if(sent < 0) {
      ESP_LOGE(TAG, "Failed to send UDP packet, retrying socket...");

      // If sending fails, close and rebuild socket
      close(sock);
      freeaddrinfo(res);
      res = nullptr;

      // Retry loop for DNS + socket
      while(true) {
        int err = getaddrinfo(host, port, &hints, &res);
        if(err != 0 || res == nullptr) {
          ESP_LOGE(TAG, "DNS retry failed: %d", err);
          vTaskDelay(pdMS_TO_TICKS(5000));
          continue;
        }
        sock = socket(res->ai_family, res->ai_socktype, 0);
        if(sock < 0) {
          ESP_LOGE(TAG, "Socket retry failed");
          freeaddrinfo(res);
          vTaskDelay(pdMS_TO_TICKS(5000));
          continue;
        }
        ESP_LOGI(TAG, "Socket re-created");
        break;
      }
    } else {
      ESP_LOGI(TAG, "Sent UDP packet #%d", counter);
    }

    vTaskDelay(pdMS_TO_TICKS(1000)); // send every second
  }
}