/**
 * @file driver.hpp
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

#pragma once
#ifndef _DRIVER_
#define _DRIVER_

#ifndef ADD_DRIVER
#define ADD_DRIVER(driver_name) (driver::driver*) (&driver_name)
#endif

#include <string_view>
#include <esp_netif.h>
#include <esp_event_base.h>

ESP_EVENT_DEFINE_BASE(UC_DRIVER_BASE);

/**
 * @brief This is the namespace containing all Driver related Code, such as:
 * driver interface, wifi driver interface, cellular driver interface
 */
namespace driver {
/**
 * @brief The types of network interfaces supported by unified comms
 */
enum InterfaceType {
  NONE,
  CELLULAR, // PPP
  WIFI,     // ESP
  ETHERNET,
  DRIVER_TYPE_COUNT,
};

enum EventType : int32_t { DISCONNECTING, CONNECTING };

enum StatusType : uint8_t { DISCONNECTED, CONNECTED };

/**
 * @brief The common interface for all drivers.
 *
 * this interface is used by the unified controller.
 *
 * it contains all the neccesary controll infrastructure such that auto failover can happen beteween
 * drivers as long as the interface requriements are properly met
 */
class Driver
{
protected:
  friend struct DriverComparator;
  friend class UnifiedController;
  esp_netif_t* network_interface;
  int priority;

public:
  esp_event_loop_handle_t eventLoop = nullptr; // handle to the unifiedController eventLoop (filled
                                               // in by the unified controller on startup)
  std::string_view name;
  InterfaceType type = DRIVER_TYPE_COUNT;
  StatusType status = DISCONNECTED;

  /**
   * @brief this function returns the configured driver priority or 0 when not configured
   */
  int getPriority() { return priority; }

  virtual esp_netif_t* getInterface() { return network_interface; };

  /**
   * @brief destroys the driver and all its internall structures.
   */
  virtual void destroy() = 0;

  /**
   * @brief prints the driver configuration
   */
  virtual void printConfig() = 0;

  /**
   * @brief This function attempts to connect the driver to the network.
   *
   * @note a valid connection is one where the netif_t has a valid IP-ADDRESS
   */
  virtual bool connect() = 0;

  /**
   * @brief This function attempts to connect the driver to the network.
   *
   * @note a valid connection is one where the netif_t has a valid IP-ADDRESS
   */
  virtual bool disconnect() = 0;

  /**
   * @brief has the driver been configured yet?
   */
  virtual bool isConfigured() = 0;

  /**
   * @brief launch the driver disconnected event, to notify the controller.
   */
  virtual void launchDisconnectedEvent() = 0;

  /**
   * @brief launch the driver connected event, to notify the
   * end user.
   */
  virtual void launchConnectedEvent() = 0;

  virtual ~Driver() = default;
};

/**
 * @brief this struct is used to compare the drivers for sorting.
 */
struct DriverComparator {
public:
  bool operator()(driver::Driver* a, driver::Driver* b) const { return a->priority < b->priority; }
};
} // namespace driver
#endif