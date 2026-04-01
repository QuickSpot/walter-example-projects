/**
 * @file gm02s.cpp
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

#include "gm02s.hpp"
#include "lwip/ip_addr.h"
#include <driver/gpio.h>
#include <esp_modem_config.h>
#include "esp_netif_ppp.h"
#include <nvs_flash.h>

driver::cellular::gm02s* modem;
namespace driver::cellular {
static void _handlePPPStatusEvent(void* arg, esp_event_base_t event_base, int32_t event_id,
                                  void* event_data)
{
  ESP_LOGD(LOGTAG, "Handling PPP status event %" PRIu32, event_id);
  switch(event_id) {
  case NETIF_PPP_ERRORUSER: {
    esp_netif_t** p_netif = (esp_netif_t**) event_data;
    ESP_LOGI(LOGTAG, "User interrupted event from netif:%p", *p_netif);
    modem->launchDisconnectedEvent();
    break;
  }
  default: {
    ESP_LOGD(LOGTAG, "Unhandled PPP status event");
    break;
  }
  }
}

gm02s::gm02s(const gm02sHardwareConfig& config) : hardwareConfig(config)
{
  name = "Sequans Monarch Modem";
  modem = this;
}

void gm02s::destroy()
{
}

void gm02s::printConfig()
{
  ESP_LOGI(LOGTAG,
           "Hardware Config:\n"
           "  - UART No: %d\n"
           "  - RX Pin: %d\n"
           "  - TX Pin: %d\n"
           "  - RTS Pin: %d\n"
           "  - CTS Pin: %d\n"
           "  - Reset Pin: %d\n"
           "  - Baud Rate: %d\n",
           hardwareConfig.uart_no, hardwareConfig.pinRX, hardwareConfig.pinTX,
           hardwareConfig.pinRTS, hardwareConfig.pinCTS, hardwareConfig.pinReset,
           hardwareConfig.baudRate);
}

// Persistent configuration: called once at boot
bool gm02s::config(std::string_view apn, int priority)
{
  if(configured)
    return true;

  esp_err_t ret;

  // Initialize network interface stack
  ret = esp_netif_init();
  if(ret != ESP_OK)
    return false;

  // Event loop (may already exist)
  ret = esp_event_loop_create_default();
  if(ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(LOGTAG, "Unable to create event loop");
    return false;
  }

  // Register PPP status event
  ret =
      esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &_handlePPPStatusEvent, this);
  if(ret != ESP_OK) {
    ESP_LOGE(LOGTAG, "Unable to register PPP status handler");
    return false;
  }

  // Setup persistent network interface
  if(!_setupNetif(priority))
    return false;

  // Setup persistent UART DTE (hardware interface)
  if(!uartDTE && !_setupUartDTE())
    return false;
  // uartDTE->set_urc_cb(callback); // overkill

  // Configure PDP context
  pdpContext.protocol_type = "IP";
  pdpContext.apn = apn;
  pdpContext.context_id = 1;

  // Setup modem DCE
  if(!_setupModemDCE())
    return false;

  Driver::priority = priority;
  configured = true;

  return true;
}

// Lightweight network connect
bool gm02s::connect()
{
  if(!configured)
    return false;

  esp_modem::command_result res;

  // Setup network URC
  res = gm02sDce->config_network_registration_urc(1);
  if(res == esp_modem::command_result::FAIL)
    return false;

  res = gm02sDce->set_radio_state(1);
  if(res == esp_modem::command_result::FAIL)
    return false;

  res = waitForConnection();
  if(res != esp_modem::command_result::OK)
    return false;

  // int state;
  // res = gm02sDce->get_network_registration_state(state);
  // if (res != esp_modem::command_result::OK) {
  //     ESP_LOGW(LOGTAG, "Failed to query network state (%d)", state);
  // } else {
  //     ESP_LOGI(LOGTAG, "Network state: %d", state);
  // }

  gm02sDce->set_mode(esp_modem::modem_mode::CMUX_MODE);

  return true;
}

bool gm02s::disconnect()
{
  gm02sDce->set_mode(esp_modem::modem_mode::COMMAND_MODE);
  if(gm02sDce->set_radio_state(0) == esp_modem::command_result::FAIL)
    return false;
  return true;
}

bool gm02s::isConfigured()
{
  return configured;
}

void gm02s::printPPPIPInfo()
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

void gm02s::launchDisconnectedEvent()
{
  if(eventLoop) {
    driver::InterfaceType iftype = this->type;
    esp_event_post_to(eventLoop, UC_DRIVER_BASE, DISCONNECTING, &iftype, sizeof(iftype), 0);
  } else {
    ESP_LOGE(LOGTAG, "invalid eventloop");
  }
}

void gm02s::launchConnectedEvent()
{
  if(eventLoop) {
    driver::InterfaceType iftype = this->type;
    esp_event_post_to(eventLoop, UC_DRIVER_BASE, CONNECTING, &iftype, sizeof(iftype), 0);
  } else {
    ESP_LOGE(LOGTAG, "invalid eventloop");
  }
}

esp_modem::command_result gm02s::at(const std::string& cmd, std::string& out, int timeout)
{
  return gm02sDce->at(cmd, out, timeout);
}

esp_modem::command_result gm02s::atRaw(const std::string& cmd, std::string& out,
                                       const std::string& pass, const std::string& fail,
                                       int timeout)
{
  return gm02sDce->at_raw(cmd, out, pass, fail, timeout);
}
esp_modem::command_result gm02s::command(const std::string& command,
                                         const std::list<std::string_view>& pass_phrase,
                                         const std::list<std::string_view>& fail_phrase,
                                         uint32_t timeout_ms)
{
  return gm02sDce->command(
      command + "\r",
      [&](uint8_t* data, size_t len) {
        std::string_view response((char*) data, len);
        if(data == nullptr || len == 0 || response.empty()) {
          return esp_modem::command_result::TIMEOUT;
        }
        for(auto& it : pass_phrase)
          if(response.find(it) != std::string::npos) {
            return esp_modem::command_result::OK;
          }
        for(auto& it : fail_phrase)
          if(response.find(it) != std::string::npos) {
            return esp_modem::command_result::FAIL;
          }
        return esp_modem::command_result::TIMEOUT;
      },
      timeout_ms);
}
esp_modem::command_result gm02s::waitForConnection()
{
  const auto pass = std::list<std::string_view>({ "+CEREG: 1", "+CEREG: 5" });
  esp_modem::command_result res = command("", pass, {}, 30 * 1000);
  if(res == esp_modem::command_result::OK) {
    ESP_LOGI(LOGTAG, "Connected to mobile network");
  } else {
    ESP_LOGW(LOGTAG, "Couldn't connect to mobile network. (Time-Out)");
  }
  return res;
}
esp_modem::SQNGM02S* gm02s::getModule()
{
  return static_cast<esp_modem::SQNGM02S*>(gm02sDce->get_module());
}
esp_modem::command_result gm02s::urcCallback(uint8_t* data, size_t len)
{
  char* mutableData = reinterpret_cast<char*>(data);

  // Log raw URC data as a bounded string (not assuming null termination)
  // ESP_LOG_BUFFER_HEXDUMP(LOGTAG, data, len, ESP_LOG_INFO);
  // ESP_LOGI(LOGTAG, "Raw URC: %.*s", static_cast<int>(len), mutableData);

  std::string_view view(mutableData, len);
  if(!view.ends_with("\r\n"))
    return esp_modem::command_result::FAIL;

  /* We iterate over the view until no more URC's have been found */
  while(!view.empty()) {

    /* Attempt to find the end of the urc*/
    size_t start = view.find("\r\n");

    if(start == std::string_view::npos) {
      break;
    }

    // Remove everything before and including first \r\n
    view.remove_prefix(start + 2);

    // Step 2: Look for the next \r\n (end of URC)
    size_t end = view.find("\r\n");

    if(end == std::string_view::npos) {
      return esp_modem::command_result::FAIL;
    }

    std::string_view line = view.substr(0, end);

    if(line.starts_with("+") && urcHandler) {
      urcHandler(line);
    }
    if(line.starts_with("+CEREG: 0")) {
      // launchDisconnectedEvent();
    }
    if(line.starts_with("+CEREG: 2")) {
      ESP_LOGD(LOGTAG, "Waiting for network...");
    }
    view.remove_prefix(end + 2); // Move past the line and \r\n
  }
  return esp_modem::command_result::OK;
}

void gm02s::_initReset()
{
  gpio_set_direction((gpio_num_t) hardwareConfig.pinReset, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode((gpio_num_t) hardwareConfig.pinReset, GPIO_FLOATING);
  gpio_deep_sleep_hold_en();

  gpio_hold_dis((gpio_num_t) hardwareConfig.pinReset);
  gpio_set_level((gpio_num_t) hardwareConfig.pinReset, 0);
  vTaskDelay(pdMS_TO_TICKS(10));
  gpio_set_level((gpio_num_t) hardwareConfig.pinReset, 1);
  gpio_hold_en((gpio_num_t) hardwareConfig.pinReset);
  vTaskDelay(pdMS_TO_TICKS(5000));
}

bool gm02s::_setupNetif(int priority)
{
  esp_netif_inherent_config_t netif_inherent_config = ESP_NETIF_INHERENT_DEFAULT_PPP();
  netif_inherent_config.route_prio = priority;
  netif_inherent_config.if_desc = "ppp_interface";

  esp_netif_config_t netif_ppp_config = {
    .base = &netif_inherent_config,
    .driver = NULL,
    .stack = ESP_NETIF_NETSTACK_DEFAULT_PPP,
  };

  network_interface = esp_netif_new(&netif_ppp_config);
  if(!network_interface) {
    ESP_LOGE("GM02S", "Unable to create PPP netif");
    return false;
  }

  return true;
}

bool gm02s::_setupUartDTE()
{
  _initReset(); // Hardware reset the modem

  esp_modem::dte_config dteConfig = { .dte_buffer_size = 4096 / 2,
                                      .task_stack_size = 4096,
                                      .task_priority = 5,
                                      .uart_config = {
                                          .port_num = hardwareConfig.uart_no,
                                          .data_bits = UART_DATA_8_BITS,
                                          .stop_bits = UART_STOP_BITS_1,
                                          .parity = UART_PARITY_DISABLE,
                                          .flow_control = ESP_MODEM_FLOW_CONTROL_HW,
                                          .source_clk = ESP_MODEM_DEFAULT_UART_CLK,
                                          .baud_rate = hardwareConfig.baudRate,
                                          .tx_io_num = hardwareConfig.pinTX,
                                          .rx_io_num = hardwareConfig.pinRX,
                                          .rts_io_num = hardwareConfig.pinRTS,
                                          .cts_io_num = hardwareConfig.pinCTS,
                                          .rx_buffer_size = 4096,
                                          .tx_buffer_size = 4096 / 2,
                                          .event_queue_size = 30,
                                      } };

  uartDTE = esp_modem::create_uart_dte(&dteConfig);
  return uartDTE != nullptr;
}

bool gm02s::_setupModemDCE()
{
  esp_modem::dce_config dceConfig = ESP_MODEM_DCE_DEFAULT_CONFIG(pdpContext.apn.data());

  gm02sDce = esp_modem::create_SQNGM02S_dce(&dceConfig, uartDTE, network_interface);

  if(!gm02sDce) {
    return false;
  }

  gm02sDce->set_urc(callback);

  /* sync communication with the modem */
  for(int i = 0; i < 3; i++) {
    if(getModule()->sync() == esp_modem::command_result::OK)
      break;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  getModule()->set_pdp_context(pdpContext);
  return true;
}
} // namespace driver::cellular
