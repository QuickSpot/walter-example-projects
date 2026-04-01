/**
 * @file gm02.hpp
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
#ifndef _SEQUANS_GM02S_
#define _SEQUANS_GM02S_

#define GM02S_DRV(cellularDriver) ((driver::cellular::gm02s*) cellularDriver)

#include <cxx_include/esp_modem_api.hpp>
#include <driver/cellularDriver.hpp>
#include <driver/uart.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <list>

namespace driver::cellular {
/**
 * @brief this is the log tag for the SQNGM02S driver.
 */
constexpr const char* LOGTAG = "[UC|GM02S]";

/**
 * @brief this struct contains all the hardware configuration options for the Sequans GM02S driver.
 */
struct gm02sHardwareConfig {
  uart_port_t uart_no;
  uint8_t pinRX;
  uint8_t pinTX;
  uint8_t pinRTS;
  uint8_t pinCTS;
  uint8_t pinReset;
  int baudRate;
};

/**
 * @brief This is the cellular driver implementation for the Sequans GM02S cellular modem
 */
class gm02s : public CellularDriver
{
public:
  gm02s(const gm02sHardwareConfig& config);

  /**
   * @brief destroys the driver instance
   */
  void destroy() override;

  /**
   * @brief print the configuration.
   */
  void printConfig() override;

  /**
   * @brief this function configures the driver
   *
   * @param[in] apn the apn to use.
   * @param priority the driver priority (see netif route_prio)
   * @param[in] dns_addr the address of the dns server to use.
   */
  bool config(std::string_view apn, int priority) override;
  bool isConfigured() override;

  /**
   * @brief this function attempts to connect the driver to the network
   *
   * @note this function cannot be blocking as the unified controller will grind to a halt.
   *
   * @note a valid connection is one where the netif_t has a valid IP-ADDRESS
   *
   * @returns True on succesful connection, False otherwise.
   */
  bool connect() override;

  /**
   * @brief this function attempts to disconnect the driver from the network
   */
  bool disconnect() override;

  void printPPPIPInfo();

  /**
   * @brief this function launches the disconected event to notify the unified controller that
   * connection was lost.
   */
  void launchDisconnectedEvent() override;

  /**
   * @brief this function launches the connected event to notify the
   * end user that connection was lost.
   */
  void launchConnectedEvent() override;

  /**
   * @brief Execute the supplied AT command
   * @param[in] cmd AT command
   * @param[out] out Command output string
   * @param[in] timeout AT command timeout in milliseconds
   * @return OK, FAIL or TIMEOUT
   */
  esp_modem::command_result at(const std::string& command, std::string& out, int timeout) override;

  /**
   * @brief Execute the supplied AT command in raw mode (doesn't append
   * '\r' to command, returns everything)
   * @param[in] cmd String command that's send to DTE
   * @param[out] out Raw output from DTE
   * @param[in] pass Pattern in response for the API to return OK
   * @param[in] fail Pattern in response for the API to return FAIL
   * @param[in] timeout AT command timeout in milliseconds
   * @return OK, FAIL or TIMEOUT
   */
  esp_modem::command_result atRaw(const std::string& cmd, std::string& out, const std::string& pass,
                                  const std::string& fail, int timeout) override;

  /**
   * @brief Execute the supplied AT command (appends the '\r')
   *
   * @param[in] command String command that's send to the uartDTE
   * @param[in] pass_phrase list containing successful return values.
   * @param[in] fail_phrase list containing failure return values.
   * @param timeout_ms The command timeout in ms
   */
  esp_modem::command_result command(const std::string& command,
                                    const std::list<std::string_view>& pass_phrase,
                                    const std::list<std::string_view>& fail_phrase,
                                    uint32_t timeout_ms);

  /**
   * @brief this function waits for a +CEREG:5 URC.
   */
  esp_modem::command_result waitForConnection();

  /**
   * @brief this modem returns the native modem instance (useful for accessing SQNGM02S specific
   * commands)
   */
  esp_modem::SQNGM02S* getModule();

  esp_modem::command_result urcCallback(uint8_t* data, size_t len);

private:
  /**
   * @brief perform an initial modem reset to tart AT communitation
   */
  void _initReset();

  /**
   * @brief setup the PPP network interface.
   */
  bool _setupNetif(int priority);

  /**
   * @brief setup the uart DTE.
   */
  bool _setupUartDTE();

  /**
   * @brief setup the modem DCE
   */
  bool _setupModemDCE();

  bool configured = false;

  esp_modem::PdpContext pdpContext { "" };
  gm02sHardwareConfig hardwareConfig = {};
  std::shared_ptr<esp_modem::DTE> uartDTE; /* DTE can be reused accross configs */
  std::unique_ptr<esp_modem::DCE> gm02sDce;
  esp_netif_inherent_config_t netifPPPInherentConfig;

  esp_modem::got_line_cb callback = [this](uint8_t* data, size_t len) {
    return this->urcCallback(data, len);
  };
};
} // namespace driver::cellular
#endif