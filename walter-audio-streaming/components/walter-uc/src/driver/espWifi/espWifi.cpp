/**
 * @file espWifi.cpp
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

#include "espWifi.hpp"
#include <cstring>
#include <esp_log.h>
#include <esp_netif_defaults.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <lwip/ip4_addr.h>
#include <lwip/ip_addr.h>

#define WIFI_STARTED_BIT (1 << 0)   // Bit 0
#define WIFI_CONNECTED_BIT (1 << 1) // Bit 1
#define WIFI_FAIL_BIT (1 << 2)      // Bit 2

static int s_retry_num = 0;

EventGroupHandle_t wifi_event_group;
driver::wifi::espWifi* router;

namespace driver::wifi {

/**
 * @brief this event handler handles all the connection related events.
 * and potentialy notifies the unified controller when neccesary.
 */
void _handleWifiEvent(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  ESP_LOGD(LOGTAG, "Handling WIFI status event %" PRIu32, event_id);
  EventBits_t bits = xEventGroupGetBits(wifi_event_group);

  switch(event_id) {
  case WIFI_EVENT_STA_START: {
    if(bits & WIFI_STARTED_BIT)
      break;
    ESP_LOGI(LOGTAG, "Wi-Fi STA started");
    xEventGroupSetBits(wifi_event_group, WIFI_STARTED_BIT);
    s_retry_num = 0;
    esp_wifi_connect();
    break;
  }
  case WIFI_EVENT_STA_STOP: {
    if(!(bits & WIFI_STARTED_BIT))
      break;
    ESP_LOGW(LOGTAG, "Wi-Fi STA stopped");
    xEventGroupClearBits(wifi_event_group, WIFI_STARTED_BIT);
    break;
  }
  case WIFI_EVENT_STA_CONNECTED: {
    if((bits & WIFI_CONNECTED_BIT))
      break;
    ESP_LOGI(LOGTAG, "Wi-Fi STA connected");
    s_retry_num = 0;
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    break;
  }
  case WIFI_EVENT_STA_DISCONNECTED: {
    if(!(bits & WIFI_STARTED_BIT))
      break;
    if(!(bits & WIFI_CONNECTED_BIT))
      break;
    ESP_LOGW(LOGTAG, "Wi-Fi STA disconnected");
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    esp_wifi_connect();
    router->launchDisconnectedEvent();
    break;
  }
  case WIFI_EVENT_AP_START: {
    ESP_LOGI(LOGTAG, "SoftAP started");
    break;
  }
  case WIFI_EVENT_AP_STACONNECTED: {
    ESP_LOGI(LOGTAG, "Client connected to AP");
    break;
  }
  case WIFI_EVENT_AP_STADISCONNECTED: {
    ESP_LOGI(LOGTAG, "Client disconnected from AP");
    break;
  }
  default: {
    ESP_LOGD(LOGTAG, "Unhandled Wi-Fi event");
    break;
  }
  }
}

espWifi::espWifi()
{
  name = "Espressif WiFi";
  router = this;
}

void espWifi::destroy()
{
  // TODO implement
}

void espWifi::printConfig()
{
  if(mode == STA) {
    ESP_LOGI(LOGTAG,
             "Wifi Config: \n"
             "  - MODE: Station\n"
             "  - SSID: %.*s\n"
             "  - PASS: HIDDEN",
             static_cast<int>(config.staConfig.ssid.length()), config.staConfig.ssid.data());
  }
}

bool espWifi::configStation(std::string_view ssid, std::string_view pass, int priority)
{
  esp_err_t ret;

  ret = nvs_flash_init();
  if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition is truncated or version mismatch, erase and retry
    nvs_flash_erase();
    ret = nvs_flash_init();
  } else if(ret != ESP_OK) {
    ESP_LOGE(LOGTAG, "Unable to init flash memory");
    return false;
  }

  ret = esp_netif_init();
  if(ret != ESP_OK)
    return false;

  wifi_event_group = xEventGroupCreate();
  if(!wifi_event_group)
    ESP_LOGE(LOGTAG, "xEventGroupCreate() fail");
  if(!wifi_event_group)
    return false;

  // Event loop could have already been initialized
  ret = esp_event_loop_create_default();
  if(ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    ESP_LOGE(LOGTAG, "esp_event_loop_create_default() fail");
  if(ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    return false;

  // network_interface = esp_netif_create_default_wifi_sta();
  if(!_setupNetif(priority)) {
    ESP_LOGW(LOGTAG, "Failed to configure network interface");
    return false;
  }

  ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &_handleWifiEvent, this,
                                            &instance_wifi);
  if(ret != ESP_OK)
    ESP_LOGE(LOGTAG, "esp_event_handler_instance_register() fail");
  if(ret != ESP_OK)
    return false;

  mode = STA;
  config.staConfig.pass = pass;
  config.staConfig.ssid = ssid;
  Driver::priority = priority;
  configured = true;
  return true;
}

bool espWifi::connect()
{
  if(!configured)
    return false;

  wifi_init_config_t initProps = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&initProps));

  wifi_config_t wifiConfig = {};
  wifiConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  wifiConfig.sta.pmf_cfg.capable = true;
  wifiConfig.sta.pmf_cfg.required = false;
  std::memcpy(wifiConfig.sta.ssid, config.staConfig.ssid.data(),
              std::min(config.staConfig.ssid.size(), sizeof(wifiConfig.sta.ssid) - 1));
  std::memcpy(wifiConfig.sta.password, config.staConfig.pass.data(),
              std::min(config.staConfig.pass.size(), sizeof(wifiConfig.sta.password) - 1));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));

  // Start Wi-Fi; event handler will call esp_wifi_connect()
  ESP_ERROR_CHECK(esp_wifi_start());

  EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

  if(bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(LOGTAG, "Wi-Fi connected successfully");
    return true;
  } else if(bits & WIFI_FAIL_BIT) {
    ESP_LOGW(LOGTAG, "Failed to connect to Wi-Fi");
    xEventGroupClearBits(wifi_event_group, WIFI_FAIL_BIT);
  }
  return false;
}

bool espWifi::disconnect()
{
  if(!configured)
    return false;

  // Cleanly disconnect + stop Wi-Fi
  esp_wifi_disconnect();
  esp_wifi_stop();

  // Deinit Wi-Fi driver to free memory
  esp_wifi_deinit();

  // Clear event bits
  xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_STARTED_BIT);

  ESP_LOGI(LOGTAG, "Wi-Fi stopped successfully");
  return true;
}

void espWifi::printIPInfo()
{
  esp_netif_ip_info_t ip_info;
  esp_netif_dns_info_t dns_info;
  esp_netif_get_ip_info(network_interface, &ip_info);

  ESP_LOGI(LOGTAG, "~~~~~~~~~~~~~~");
  ESP_LOGI(LOGTAG, "IP          : " IPSTR, IP2STR(&ip_info.ip));
  ESP_LOGI(LOGTAG, "Netmask     : " IPSTR, IP2STR(&ip_info.netmask));
  ESP_LOGI(LOGTAG, "Gateway     : " IPSTR, IP2STR(&ip_info.gw));
  esp_netif_get_dns_info(network_interface, (esp_netif_dns_type_t) 0, &dns_info);
  ESP_LOGI(LOGTAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
  esp_netif_get_dns_info(network_interface, (esp_netif_dns_type_t) 1, &dns_info);
  ESP_LOGI(LOGTAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
  ESP_LOGI(LOGTAG, "~~~~~~~~~~~~~~");
}

bool espWifi::isConfigured()
{
  return configured;
}
void espWifi::launchDisconnectedEvent()
{
  driver::InterfaceType iftype = this->type;
  esp_event_post_to(eventLoop, UC_DRIVER_BASE, DISCONNECTING, &iftype, sizeof(iftype), 0);
}
void espWifi::launchConnectedEvent()
{
  driver::InterfaceType iftype = this->type;
  esp_event_post_to(eventLoop, UC_DRIVER_BASE, CONNECTING, &iftype, sizeof(iftype), 0);
}
bool espWifi::_setupNetif(int prio)
{
  esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
  base_cfg.route_prio = prio;
  base_cfg.if_desc = "wifi_interface";

  esp_netif_config_t cfg = { .base = &base_cfg,
                             .driver = NULL,
                             .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA };

  // 1. Create the netif
  network_interface = esp_netif_new(&cfg);
  if(!network_interface) {
    return false;
  }

  // 2. Attach Wi-Fi driver (STA instance)
  ESP_ERROR_CHECK(esp_netif_attach_wifi_station(network_interface));

  esp_wifi_set_default_wifi_sta_handlers();
  return true;
}
} // namespace driver::wifi