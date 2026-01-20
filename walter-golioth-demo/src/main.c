/*
 * Copyright (c) 2024 DPTechnics bv
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(walter_golioth_demo, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/stream.h>
#include <samples/common/net_connect.h>
#include <samples/common/sample_credentials.h>
#include <zcbor_encode.h>   
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>

#include "sensor_dht20.h"

#define LOOP_DELAY_S 5

struct golioth_client *client;
static K_SEM_DEFINE(connected, 0, 1);

static const struct gpio_dt_spec power_3v3_switch =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(power_3v3_switch), gpios, {0});

static void on_client_event(struct golioth_client *client,
			    enum golioth_client_event event,
			    void *arg)
{
	bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
	if (is_connected) {
		k_sem_give(&connected);
	}
	LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

static void start_golioth_client(void)
{
	/* Get the client configuration from auto-loaded settings */
	const struct golioth_client_config *client_config = golioth_sample_credentials_get();

	/* Create and start a Golioth Client */
	client = golioth_client_create(client_config);

	/* Register Golioth on_connect callback */
	golioth_client_register_event_callback(client, on_client_event, NULL);
}

static void initialize_3v3_output(void)
{
	int err;

	LOG_DBG("Enabling Walter's 3.3V DC output...");
	if (!gpio_is_ready_dt(&power_3v3_switch)) {
		LOG_ERR("The GPIO port is not ready");
		return;
	}

	err = gpio_pin_configure_dt(&power_3v3_switch, GPIO_OUTPUT_ACTIVE);
	if (err) {
		LOG_ERR("Configuring GPIO pin failed: %d", err);
		return;
	}
}

int main(void)
{
	LOG_DBG("Start Walter Golioth demo");

	/* Enable 3.3V DC output */
	initialize_3v3_output();

	/* Initialize DHT20 sensor */
	dht20_sensor_init();

	/* Initialize modem PPP mode */
	IF_ENABLED(CONFIG_GOLIOTH_SAMPLE_COMMON, (net_connect();))

	/* Start Golioth client */
	start_golioth_client();

	/* Block until connected to Golioth */
	k_sem_take(&connected, K_FOREVER);

	while (true)
	{
		int err;
		uint8_t buf[30];
		static struct dht20_sensor_measurement dht20_sm;

		/* Read the DHT20 sensor */
		LOG_DBG("Collecting DHT20 measurements...");
		err = dht20_sensor_read(&dht20_sm);
		if (err) {
			LOG_ERR("Failed to read from DHT20 sensor: %d", err);
		} else {
			dht20_log_measurements(&dht20_sm);
		}

		/* Send sensor data to Golioth */
		if (golioth_client_is_connected(client)) {
			LOG_DBG("Sending sensor data to Golioth");

			/* Create zcbor state variable for encoding */
			ZCBOR_STATE_E(zse, 1, buf, sizeof(buf), 1);

			/* Encode the CBOR map header */
			bool ok = zcbor_map_start_encode(zse, 1);
			if (!ok)
			{
				LOG_ERR("Failed to start encoding CBOR map");
			}

			/* Encode the map key "temp" into buf as a text string */
			ok = zcbor_tstr_put_lit(zse, "temp");
			if (!ok)
			{
				LOG_ERR("CBOR: Failed to encode temp name");
			}

			/* Encode the temperature value into buf as a float */
			ok = zcbor_float32_put(zse, sensor_value_to_float(&dht20_sm.temperature));
			if (!ok)
			{
				LOG_ERR("CBOR: failed to encode temp value");
			}

			/* Encode the map key "hum" into buf as a text string */
			ok = zcbor_tstr_put_lit(zse, "hum");
			if (!ok)
			{
				LOG_ERR("CBOR: Failed to encode hum name");
			}

			/* Encode the temperature value into buf as a float */
			ok = zcbor_float32_put(zse, sensor_value_to_float(&dht20_sm.humidity));
			if (!ok)
			{
				LOG_ERR("CBOR: failed to encode hum value");
			}

			/* Close the CBOR map */
			ok = zcbor_map_end_encode(zse, 1);
			if (!ok)
			{
				LOG_ERR("Failed to close CBOR map");
			}

			size_t payload_size = (intptr_t) zse->payload - (intptr_t) buf;

			err = golioth_stream_set_sync(client,
						      "sensor",
						      GOLIOTH_CONTENT_TYPE_CBOR,
						      buf,
						      payload_size,
						      3);
			if (err) {
				LOG_ERR("Failed to send sensor data to Golioth: %d", err);
			}
		} else {
			LOG_WRN("Device is not connected to Golioth, unable to send sensor data");
		}

		k_sleep(K_SECONDS(LOOP_DELAY_S));
	}

	return 0;
}
