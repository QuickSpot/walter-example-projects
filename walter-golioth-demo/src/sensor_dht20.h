/*
 * Copyright (c) 2024 DPTechnics bv
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SENSOR_DHT20_H__
#define __SENSOR_DHT20_H__

#include <zephyr/drivers/sensor.h>

struct dht20_sensor_measurement {
	struct sensor_value temperature;
	struct sensor_value humidity;
};

int dht20_sensor_init(void);
int dht20_sensor_read(struct dht20_sensor_measurement *measurement);
void dht20_log_measurements(struct dht20_sensor_measurement *measurement);

#endif