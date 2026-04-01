/**
 * @file cellularDriver.hpp
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
#ifndef _CELLULAR_DRIVER_
#define _CELLULAR_DRIVER_
#include <cxx_include/esp_modem_types.hpp>
#include "driver.hpp"

/**
 * @brief The namespace for all cellular driver related code
 */
namespace driver::cellular {
/**
 * @brief header for urc handler.
 */
using UrcLineHandler = void (*)(std::string_view);

/**
 * @brief this is the commonn interface for all the Celullar drivers.
 *
 * it also contains functions for sending commands to the modem.
 * the URC's are seperated in the driver.
 */
class CellularDriver : public Driver
{
public:
  CellularDriver() { type = CELLULAR; }

  /**
   * @brief This confiures the cellular driver
   *
   * @param[in] apn the apn to connect to.
   * @param[in] priority the driver priority.
   */
  virtual bool config(std::string_view apn, int priority) = 0;

  /**
   * @brief Execute the supplied AT command
   * @param[in] cmd AT command
   * @param[out] out Command output string
   * @param[in] timeout AT command timeout in milliseconds
   * @return OK, FAIL or TIMEOUT
   */
  virtual esp_modem::command_result at(const std::string& command, std::string& out,
                                       int timeout) = 0;

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
  virtual esp_modem::command_result atRaw(const std::string& cmd, std::string& out,
                                          const std::string& pass, const std::string& fail,
                                          int timeout) = 0;

  /**
   * @brief this function sets the user URC handler.
   *
   * @param[in] handler the function pointer to the handler.
   */
  virtual void setUrcHandler(UrcLineHandler handler) { urcHandler = handler; }

protected:
  UrcLineHandler urcHandler = nullptr; /* function pointer to the urc_handler*/
};
} // namespace driver::cellular
#endif