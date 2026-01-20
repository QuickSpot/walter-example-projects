#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_dht20, LOG_LEVEL_DBG);

#include <zephyr/drivers/sensor.h>

#include "sensor_dht20.h"

const struct device *dht20_dev = DEVICE_DT_GET(DT_NODELABEL(dht20));

int dht20_sensor_init(void)
{
	LOG_DBG("Initializing DHT20 weather sensor");

	if (dht20_dev == NULL) {
		/* No such node, or the node does not have status "okay". */
		LOG_ERR("Device \"%s\" not found", dht20_dev->name);
		return -ENODEV;
	}

	if (!device_is_ready(dht20_dev)) {
		LOG_ERR("Device \"%s\" is not ready", dht20_dev->name);
		return -ENODEV;
	}

	return 0;
}

int dht20_sensor_read(struct dht20_sensor_measurement *measurement)
{
	int err;

	LOG_DBG("Reading DHT20 weather sensor");

	err = sensor_sample_fetch(dht20_dev);
	if (err) {
		LOG_ERR("Error fetching sensor sample: %d", err);
		return err;
	}

	sensor_channel_get(dht20_dev, SENSOR_CHAN_AMBIENT_TEMP, &measurement->temperature);
	sensor_channel_get(dht20_dev, SENSOR_CHAN_HUMIDITY, &measurement->humidity);

	return err;
}

void dht20_log_measurements(struct dht20_sensor_measurement *measurement)
{
	LOG_DBG("DHT20: Temperature=%.2f °C, Humidity=%.2f %%RH",
		sensor_value_to_double(&measurement->temperature),
		sensor_value_to_double(&measurement->humidity));
}