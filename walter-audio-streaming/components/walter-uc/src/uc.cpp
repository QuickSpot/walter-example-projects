/**
 * @file uc.cpp
 * @author Daan Pape <daan@dptechnics.com>, Arnoud Devoogdt <arnoud@dptechnics.com>, Robbe Beernaert
 * @date 8 Aug 2025
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

#include "uc.hpp"
#include <algorithm>
#include <driver/wifiDriver.hpp>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_log.h>

#define PPP_IP_BIT (1 << 0)
#define STA_IP_BIT (1 << 1)
#define ETH_IP_BIT (1 << 2)

EventGroupHandle_t ip_event_group;

EventBits_t interfaceTypeToBit(driver::InterfaceType type)
{
  switch(type) {
  case driver::WIFI:
    return STA_IP_BIT;
  case driver::ETHERNET:
    return ETH_IP_BIT;
  case driver::CELLULAR:
    return PPP_IP_BIT;
  default:
    return 0;
  }
}

namespace {
void unified_controller_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data)
{
  UnifiedController* self = static_cast<UnifiedController*>(arg);
  self->driverEventHandler(base, id, data);
}

static void _handleIpEvent(void* handler_args, esp_event_base_t base, int32_t event_id, void* data)
{
  ESP_LOGD(LOGTAG, "Handling IP event %" PRIu32, event_id);
  UnifiedController* controller = (UnifiedController*) handler_args;
  EventBits_t bits = xEventGroupGetBits(ip_event_group);

  switch(event_id) {
  case IP_EVENT_STA_GOT_IP: {
    if(bits & STA_IP_BIT)
      break;

    ip_event_got_ip_t* event = (ip_event_got_ip_t*) data;
    esp_netif_t* netif = event->esp_netif;
    esp_netif_dns_info_t dns_info;
    esp_netif_get_dns_info(netif, (esp_netif_dns_type_t) 0, &dns_info);
    esp_netif_get_dns_info(netif, (esp_netif_dns_type_t) 1, &dns_info);

    ESP_LOGI(LOGTAG, "Station received IP address");
    ESP_LOGI(LOGTAG, "~~~~~~~~~~~~~~");
    ESP_LOGI(LOGTAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(LOGTAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
    ESP_LOGI(LOGTAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
    ESP_LOGI(LOGTAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    ESP_LOGI(LOGTAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    ESP_LOGI(LOGTAG, "~~~~~~~~~~~~~~");

    xEventGroupSetBits(ip_event_group, STA_IP_BIT);
    break;
  }
  case IP_EVENT_STA_LOST_IP: {
    if(!(bits & STA_IP_BIT))
      break;

    ESP_LOGW(LOGTAG, "Station lost IP address");

    xEventGroupClearBits(ip_event_group, STA_IP_BIT);
    controller->triggerReconnect();
    break;
  }
  case IP_EVENT_AP_STAIPASSIGNED: {
    break;
  }
  case IP_EVENT_GOT_IP6: {
    ip_event_got_ip6_t* event6 = (ip_event_got_ip6_t*) data;
    ESP_LOGI(LOGTAG, "Got IPv6 address " IPV6STR, IPV62STR(event6->ip6_info.ip));
    break;
  }
  case IP_EVENT_ETH_GOT_IP: {
    if(bits & ETH_IP_BIT)
      break;

    ESP_LOGW(LOGTAG, "Ethernet received IP address");

    xEventGroupSetBits(ip_event_group, ETH_IP_BIT);
    break;
  }
  case IP_EVENT_ETH_LOST_IP: {
    if(!(bits & ETH_IP_BIT))
      break;

    ESP_LOGW(LOGTAG, "Ethernet lost IP address");

    xEventGroupClearBits(ip_event_group, ETH_IP_BIT);
    controller->triggerReconnect();
    break;
  }
  case IP_EVENT_PPP_GOT_IP: {
    if(bits & PPP_IP_BIT)
      break;

    ip_event_got_ip_t* event = (ip_event_got_ip_t*) data;
    esp_netif_t* netif = event->esp_netif;
    esp_netif_dns_info_t dns_info;
    esp_netif_get_dns_info(netif, (esp_netif_dns_type_t) 0, &dns_info);
    esp_netif_get_dns_info(netif, (esp_netif_dns_type_t) 1, &dns_info);

    ESP_LOGI(LOGTAG, "PPP Interface received IP address");
    ESP_LOGI(LOGTAG, "~~~~~~~~~~~~~~");
    ESP_LOGI(LOGTAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(LOGTAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
    ESP_LOGI(LOGTAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
    ESP_LOGI(LOGTAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    ESP_LOGI(LOGTAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    ESP_LOGI(LOGTAG, "~~~~~~~~~~~~~~");

    xEventGroupSetBits(ip_event_group, PPP_IP_BIT);
    break;
  }
  case IP_EVENT_PPP_LOST_IP: {
    if(!(bits & PPP_IP_BIT))
      break;

    ESP_LOGW(LOGTAG, "PPP Interface lost IP address");

    xEventGroupClearBits(ip_event_group, PPP_IP_BIT);
    controller->triggerReconnect();
    break;
  }
  case IP_EVENT_TX_RX:
  default: {
    break;
  }
  }
}
} // namespace

UnifiedController::UnifiedController(sUnifiedCommInternal* boardConfig) : _board_config(boardConfig)
{
  init = true;
  ESP_LOGD(LOGTAG, "initalized unified comms for %s.", boardConfig->name.data());
}

void UnifiedController::printConfig()
{
  uint8_t offset = 0;
  driver::Driver* driver = _board_config->drivers[offset];
  while(driver != nullptr) {
    driver->printConfig();
    driver = _board_config->drivers[++offset];
  }
}

bool UnifiedController::start()
{
  if(_board_config->drivers[0] == nullptr) {
    ESP_LOGE(LOGTAG,
             "No drivers configured! Please configure at least one for unified comms to work.");
    return false;
  }

  vTaskDelay(pdMS_TO_TICKS(1000));

  ESP_LOGI(LOGTAG, "Started unified controller");
  if(!_createEventLoop()) {
    ESP_LOGD(LOGTAG, "unable to create controller event loop");
    return false;
  }

  // Event loop could have already been initialized
  esp_err_t ret = esp_event_loop_create_default();
  if(ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    ESP_LOGE(LOGTAG, "Unable to create event loop");
  if(ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    return false;

  ret = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &_handleIpEvent, this);
  if(ret != ESP_OK)
    ESP_LOGE(LOGTAG, "Unable to register IP event handler");
  if(ret != ESP_OK)
    return false;

  ip_event_group = xEventGroupCreate();
  if(!ip_event_group)
    ESP_LOGE(LOGTAG, "Unable to create event group");
  if(!ip_event_group)
    return false;

  xTaskCreate(&UnifiedController::_ucTask, "Unified Controller Task", 4096, this, 5,
              &_uc_task_handle);

  if(_uc_task_handle == nullptr) {
    ESP_LOGW(LOGTAG, "Couldn't start the unified controller task");
    return false;
  }

  triggerReconnect();

  return true;
}

void UnifiedController::connectBestDriver()
{
  ESP_LOGI(LOGTAG, "(Re)connecting drivers");

  driver::Driver* bestDriver = nullptr;

  // Iterate through every driver starting with the highest priority, until one is connected
  for(driver::Driver** current = _board_config->drivers;
      current != nullptr && (*current) != nullptr && (bestDriver == nullptr); current++) {
    auto driver = *current;

    if(!driver->isConfigured()) {
      ESP_LOGW(LOGTAG, "%.*s is not configured", (int) driver->name.size(), driver->name.data());
      continue;
    }

    EventBits_t driverBit = interfaceTypeToBit(driver->type);
    if(driverBit == 0) {
      ESP_LOGW(LOGTAG, "%.*s has invalid interface type %d", (int) driver->name.size(),
               driver->name.data(), driver->type);
      continue;
    }

    // Skip if driver already has IP
    EventBits_t bits = xEventGroupGetBits(ip_event_group);
    if(bits & driverBit) {
      ESP_LOGI(LOGTAG, "%.*s already connected", (int) driver->name.size(), driver->name.data());
      bestDriver = driver;
      continue;
    }

    if(!driver->connect()) {
      ESP_LOGW(LOGTAG, "%.*s couldn't connect", (int) driver->name.size(), driver->name.data());
      driver->disconnect();
      continue;
    }

    // Wait for IP on this driver's interface
    bits = xEventGroupWaitBits(ip_event_group, driverBit, pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));

    if(bits & driverBit) {
      bestDriver = driver;
      ESP_LOGI(LOGTAG, "%.*s connected", (int) driver->name.size(), driver->name.data());
    } else {
      ESP_LOGW(LOGTAG, "%.*s connected but never received an IP", (int) driver->name.size(),
               driver->name.data());
      driver->disconnect();
    }
  }

  _selected_driver = bestDriver;
}

void UnifiedController::disconnectUnselectedDrivers()
{
  driver::Driver** current = _board_config->drivers;

  while(current != nullptr && (*current) != nullptr) {
    auto driver = *current;

    if(driver == _selected_driver) {
      current++;
      continue;
    }

    if(driver->isConfigured()) {
      bool success = driver->disconnect();
      if(!success) {
        ESP_LOGW(LOGTAG, "Couldn't disconnect driver %.*s", (int) driver->name.size(),
                 driver->name.data());
      }
    }

    current++;
  }
}

esp_err_t UnifiedController::registerEventHandler(driver::EventType event_id,
                                                  esp_event_handler_t event_handler,
                                                  void* event_handler_arg)
{
  return esp_event_handler_register(UC_DRIVER_BASE, event_id, event_handler, event_handler_arg);
}

void UnifiedController::_sortDrivers()
{
  if(_board_config != nullptr) {
    /* First, find number of drivers */
    size_t count = 0;
    while(_board_config->drivers[count] != nullptr) {
      ++count;
    }

    /* Now sort them in-place */
    std::sort(_board_config->drivers, _board_config->drivers + count, driver::DriverComparator {});

    // Sort drivers based on priority
    ESP_LOGD(LOGTAG, "Sorted drivers");
  }
}

bool UnifiedController::_createEventLoop()
{
  if(_event_loop) {
    /* destroy previous event loop when applicable */
    _destroyEventLoop();
  }
  esp_event_loop_args_t loopArgs = {
    .queue_size = 12,
    .task_name = "UnifiedController",
    .task_priority = 5,
    .task_stack_size = 4096,
    .task_core_id = 0 /* let ESP choose*/
  };

  esp_err_t eventCreateRes = esp_event_loop_create(&loopArgs, &_event_loop);

  if(eventCreateRes != ESP_OK) {
    ESP_LOGE(LOGTAG, "unable to create the Unified Event Loop: esp_err_t(%i)", eventCreateRes);
    return false;
  }

  size_t count = 0;
  while(_board_config->drivers[count] != nullptr) {
    if(_board_config->drivers[count]) {
      _board_config->drivers[count]->eventLoop = _event_loop;
    }
    ++count;
  }

  esp_event_handler_register_with(_event_loop, UC_DRIVER_BASE, ESP_EVENT_ANY_ID,
                                  unified_controller_event_handler, this);

  ESP_LOGD(LOGTAG, "created Unified Event Loop");
  return true;
}

void UnifiedController::_destroyEventLoop()
{
  if(_event_loop) {
    esp_err_t eventDestroyRes = esp_event_loop_delete(_event_loop);

    if(eventDestroyRes != ESP_OK) {
      ESP_LOGE(LOGTAG, "unable to destroy default event loop");
    } else {
      _event_loop = nullptr;
    }
  }
}

void UnifiedController::triggerReconnect()
{
  _reconnect_time = ReconnectTimer::ShortPoll;

  // Resume the task if it's suspended
  vTaskResume(_uc_task_handle);
  // interupt current delay and start connecting immediately
  xTaskNotifyGive(_uc_task_handle);
}

void UnifiedController::_ucTask(void* pvParameters)
{
  UnifiedController* self = static_cast<UnifiedController*>(pvParameters);

  while(true) {
    if(self->_reconnect_time == ReconnectTimer::None) {
      vTaskSuspend(nullptr); // Suspend until resumed
      continue;
    }

    self->_sortDrivers();
    self->connectBestDriver();

    driver::Driver* drv = self->_selected_driver;

    if(drv != nullptr) {
      if(drv->getPriority() == self->_board_config->drivers[0]->getPriority()) {
        // An active network driver that is the highest priority should remain active as long as
        // possible
        self->_reconnect_time = ReconnectTimer::None;
      } else {
        // An active network driver that isn't the highest priority will re-evaluate once in a while
        self->_reconnect_time = ReconnectTimer::LongPoll;
      }

      ESP_LOGI(LOGTAG,
               " === Selecting [ %.*s ] as default network driver === ", (int) drv->name.size(),
               drv->name.data());
      esp_netif_set_default_netif(drv->getInterface());
      self->disconnectUnselectedDrivers();

    } else {
      // No active network driver means we should try to connect again soon
      self->_reconnect_time = ReconnectTimer::ShortPoll;
      ESP_LOGW(LOGTAG, "No driver was able to connect to the network. Retrying soon...");
    }

    // Wait for next reconnect, but can be woken up immediately
    TickType_t delay_ticks = 0;
    switch(self->_reconnect_time) {
    case ReconnectTimer::ShortPoll:
      delay_ticks = pdMS_TO_TICKS(20 * 1000);
      break;
    case ReconnectTimer::LongPoll:
      delay_ticks = pdMS_TO_TICKS(5 * 60 * 1000);
      break;
    default:
      delay_ticks = portMAX_DELAY;
      break;
    }

    // Wait, but allow immediate wake via notification
    ulTaskNotifyTake(pdTRUE, delay_ticks);
  }
}

void UnifiedController::driverEventHandler(esp_event_base_t event_base, int32_t event_id,
                                           void* event_data)
{

  driver::InterfaceType* iftype = (driver::InterfaceType*) event_data;

  switch(event_id) {
  case driver::CONNECTING: {
    if(*iftype == driver::InterfaceType::CELLULAR) {

    } else if(*iftype == driver::InterfaceType::WIFI) {

    } else if(*iftype == driver::InterfaceType::ETHERNET) {
    }
    break;
  }

  case driver::DISCONNECTING: {
    if(*iftype == driver::InterfaceType::CELLULAR) {
      if(!(xEventGroupGetBits(ip_event_group) & PPP_IP_BIT))
        break;

      ESP_LOGW(LOGTAG, "CELLULAR Interface disconnected from the network");

      xEventGroupClearBits(ip_event_group, PPP_IP_BIT);
      triggerReconnect();
    } else if(*iftype == driver::InterfaceType::WIFI) {
      if(!(xEventGroupGetBits(ip_event_group) & STA_IP_BIT))
        break;

      ESP_LOGW(LOGTAG, "WIFI Interface disconnected from the network");

      xEventGroupClearBits(ip_event_group, STA_IP_BIT);
      triggerReconnect();
    } else if(*iftype == driver::InterfaceType::ETHERNET) {
      if(!(xEventGroupGetBits(ip_event_group) & ETH_IP_BIT))
        break;

      ESP_LOGW(LOGTAG, "ETHERNET Interface disconnected from the network");

      xEventGroupClearBits(ip_event_group, ETH_IP_BIT);
      triggerReconnect();
    }
    break;
  }

  default: {
    ESP_LOGD(LOGTAG, "Unknown driver event");
    break;
  }
  }
}