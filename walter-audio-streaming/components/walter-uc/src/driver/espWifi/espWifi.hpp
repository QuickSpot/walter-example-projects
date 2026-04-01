/**
 * @file espWifi.hpp
 * @author Daan Pape <daan@dptechnics.com>, Arnoud Devoogdt <arnoud@dptechnics.com>, Robbe Beernaert
 * @date 21 Aug 2025
 * @copyright DPTechnics bv
 * @brief Walter unified comms library
 *
 * @section LICENSE
 *
 * Copyright (C) 2025, DPTechnics bv
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *      conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list of
 *      conditions and the following disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *   3. Neither the name of DPTechnics bv nor the names of its contributors may be used to endorse
 *      or promote products derived from this software without specific prior written permission.
 *
 *   4. This software, with or without modification, must only be used with a Walter board from
 *      DPTechnics bv.
 *
 *   5. Any software provided in binary form under this license must not be reverse engineered,
 *      decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY DPTECHNICS BV “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT, AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL DPTECHNICS BV OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @section DESCRIPTION
 *
 * This file contains the unified comms library for Walter.
 */

#pragma once
#ifndef _ESP_WIFI_DRIVER_
#define _ESP_WIFI_DRIVER_
#include <driver/wifiDriver.hpp>
#include <esp_netif.h>
#include "freertos/FreeRTOS.h"

#include "freertos/event_groups.h"
namespace driver::wifi {
constexpr const char* LOGTAG = "[UC|ESPWIFI]";

/**
 * @brief this is the driver implementation for ESP-WIFI
 */
class espWifi : public WifiDriver
{
public:
  bool configured = false;

  espWifi();

  /**
   * @brief cleans up the driver to a clean state.
   */
  void destroy() override;

  /**
   * @brief this function prints the current configuration of the wifi driver.
   */
  void printConfig() override;

  /**
   * @brief configure the
   */
  bool configStation(std::string_view ssid, std::string_view pass, int priority) override;

  /**
   * @brief connect the ESP-WIFI driver to the network.
   */
  bool connect() override;

  /**
   * @brief disconnect the ESP-WIFI driver from the network.
   */
  bool disconnect() override;

  void printIPInfo() override;

  /**
   * @brief has the driver been configured.
   */
  bool isConfigured() override;

  /**
   * @brief this function launches the disconected event to notify the
   * unified controller that connection was lost.
   */
  void launchDisconnectedEvent() override;

  /**
   * @brief this function launches the connected event to notify the
   * end user that connection was lost.
   */
  void launchConnectedEvent() override;

private:
  /**
   * @brief setup the WiFi network interface.
   */
  bool _setupNetif(int prio);

  struct StationConfig {
    std::string_view ssid;
    std::string_view pass;
  };
  struct espWifiConfig {
    StationConfig staConfig;
    // TODO add other confg structs.
  } config;

  esp_event_handler_instance_t instance_wifi;   // instance reference for the wifi event handler.
  esp_event_handler_instance_t instance_got_ip; // instance reference for the ip event handler.
};
} // namespace driver::wifi

#endif