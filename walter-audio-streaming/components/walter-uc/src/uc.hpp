/**
 * @file uc.hpp
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

#ifndef _UNIFIED_COMMS_H_
#define _UNIFIED_COMMS_H_

#include <driver/driver.hpp>
#include <esp_event_base.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
constexpr const char* LOGTAG = "[Unified Controller]";
struct sUnifiedCommInternal {
  std::string_view name;
  driver::Driver* drivers[];
};

enum class ReconnectTimer { None, ShortPoll, LongPoll };

class UnifiedController
{
public:
  UnifiedController() = default; // TODO implement cleanup
  UnifiedController(sUnifiedCommInternal* boardConfig);

  /**
   * @brief This function prints the configuration for each driver
   */
  void printConfig();

  /**
   * @brief This starts the unified comms network controller.
   */
  bool start();

  /**
   * @brief this is the handler function that handles connection events and handles reconnection.
   */
  void driverEventHandler(esp_event_base_t event_base, int32_t event_id, void* event_data);

  /**
   * @brief attempt to connect the highest priority driver to the network
   */
  void connectBestDriver();

  void disconnectUnselectedDrivers();

  /**
   * @brief this function registers an event handler to the unifiedController
   * event loop.
   *
   * @param[in] event_id the ID of the event to register the handler for
   * @param[in] event_handler the handler function which gets called when the
   * event is dispatched
   * @param[in] event_handler_arg event_handler_arg data, aside from event
   * data, that is passed to the handler when it is called
   */
  esp_err_t registerEventHandler(driver::EventType event_id, esp_event_handler_t event_handler,
                                 void* event_handler_arg = nullptr);

  /**
   * @brief instantly trigger a reconnection logic and set the poll interval to short
   */
  void triggerReconnect();

private:
  /**
   * @brief This function sorts the drivers based on their priority
   */
  void _sortDrivers();

  /**
   * @brief creates the default event loop
   */
  bool _createEventLoop();

  /**
   *  @brief destroys the internal event loop.
   */
  void _destroyEventLoop();

  /**
   * @brief the reconnection FreeRTOS task
   */
  static void _ucTask(void* param);

  sUnifiedCommInternal* _board_config; /* The reference to the board confiuration */
  static inline bool init; /* There can ever be only 1 instance of UnifiedController at a time. */
  esp_event_loop_handle_t _event_loop = nullptr; /* eventLoop => nullptr, not initialized */
  static inline TaskHandle_t _uc_task_handle = nullptr;
  ReconnectTimer _reconnect_time = ReconnectTimer::None;
  driver::Driver* _selected_driver = nullptr;
};

#endif