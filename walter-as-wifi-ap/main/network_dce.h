/**
 * @file network_dce.h
 * @author Robbe Beernaert <robbe@dptechnics.com>
 * @author Thibo Verheyde <thibo@dptechnics.com>
 * @date 19 March 2026
 * @copyright DPTechnics bv
 * @brief Public API for the Sequans GM02SP modem DCE wrapper
 *
 * @section LICENSE
 *
 * Copyright (C) 2026, DPTechnics bv
 * All rights reserved.
 *
  * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * @section DESCRIPTION
 *
 * Declares the modem lifecycle functions used by ap_to_pppos.c to manage the
 * Sequans GM02SP LTE modem: initialisation with a PPPoS netif, hard reset,
 * entering and leaving data mode, AT sync check, and signal quality query.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Initialize a singleton covering the PPP network provided by the connected modem device
 *
 * @param netif Already created network interface in PPP mode
 *
 * @return ESP_OK on success
 */
esp_err_t modem_init_network(esp_netif_t *netif);

/**
 * @brief Destroys the single network DCE
 */
void modem_deinit_network();

/**
 * @brief Starts the PPP network
 */
bool modem_start_network();

/**
 * @brief Stops the PPP network
 */
bool modem_stop_network();

bool modem_check_sync();

void modem_reset();

bool modem_check_signal();

#ifdef __cplusplus
}
#endif
