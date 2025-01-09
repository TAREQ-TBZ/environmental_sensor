/*
 * Copyright (c) 2024 Tareq Mhisen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <ram_pwrdn.h>

#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_nrf_platform.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>

#include "humidity_temperature_svc.h"
#include "user_interface.h"
#include "zb_environmental_sensor.h"
#include "zigbee_svc.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zigbee_svc, LOG_LEVEL_DBG);

#define MEASUREMENT_PERIOD_MSEC      (1000 * CONFIG_MEASURING_PERIOD_SECONDS)
#define FIRST_MEASUREMENT_DELAY_MSEC (1000 * CONFIG_FIRST_MEASUREMENT_DELAY_SECONDS)
#define KEEP_ALIVE_PERIOD_MSEC       (1000 * CONFIG_KEEP_ALIVE_PERIOD_SECONDS)
#define LONG_POLL_PERIOD_MSEC        (1000 * CONFIG_LONG_POLL_PERIOD_SECONDS)

/* Stores all cluster-related attributes */
static struct zb_device_ctx dev_ctx;

/* Declare attribute list for Basic cluster */
ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(basic_attr_list, &dev_ctx.basic_attr.zcl_version, NULL, NULL,
				     NULL, dev_ctx.basic_attr.mf_name, dev_ctx.basic_attr.model_id,
				     dev_ctx.basic_attr.date_code, &dev_ctx.basic_attr.power_source,
				     NULL, NULL, NULL);

/* Declare attribute list for Identify cluster (server). */
ZB_ZCL_DECLARE_IDENTIFY_SERVER_ATTRIB_LIST(identify_server_attr_list,
					   &dev_ctx.identify_attr.identify_time);

/* Declare attribute list for temperature cluster */
ZB_ZCL_DECLARE_TEMP_MEASUREMENT_ATTRIB_LIST(temperature_measurement_attr_list,
					    &dev_ctx.temp_attrs.measure_value,
					    &dev_ctx.temp_attrs.min_measure_value,
					    &dev_ctx.temp_attrs.max_measure_value,
					    &dev_ctx.temp_attrs.tolerance);

/* Declare attribute list for humidity cluster */
ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST(humidity_measurement_attr_list,
						    &dev_ctx.humidity_attrs.measure_value,
						    &dev_ctx.humidity_attrs.min_measure_value,
						    &dev_ctx.humidity_attrs.max_measure_value);

/* Clusters setup */
ZB_HA_DECLARE_ENVIRONMENTAL_SENSOR_CLUSTER_LIST(environmental_sensor_cluster_list, basic_attr_list,
						identify_server_attr_list,
						temperature_measurement_attr_list,
						humidity_measurement_attr_list);

/* Endpoint setup (single) */
ZB_HA_DECLARE_ENVIRONMENTAL_SENSOR_EP(environmental_sensor_ep, ENVIRONMENTAL_SENSOR_ENDPOINT_NB,
				      environmental_sensor_cluster_list);

/* Device context */
ZBOSS_DECLARE_DEVICE_CTX_1_EP(environmental_sensor_ctx, environmental_sensor_ep);

void zigbee_svc_clusters_init(void)
{
	/* Basic cluster attributes */
	dev_ctx.basic_attr.zcl_version = ZB_ZCL_VERSION;
	dev_ctx.basic_attr.power_source = ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;

	/* Use ZB_ZCL_SET_STRING_VAL to set strings, because the first byte
	 * should contain string length without trailing zero.
	 *
	 * For example "test" string will be encoded as:
	 *   [(0x4), 't', 'e', 's', 't']
	 */
	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.mf_name, CONFIG_SENSOR_INIT_BASIC_MANUF_NAME,
			      ZB_ZCL_STRING_CONST_SIZE(CONFIG_SENSOR_INIT_BASIC_MANUF_NAME));

	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.model_id, CONFIG_SENSOR_INIT_BASIC_MODEL_ID,
			      ZB_ZCL_STRING_CONST_SIZE(CONFIG_SENSOR_INIT_BASIC_MODEL_ID));

	/* Identify cluster attributes */
	dev_ctx.identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

	/* Temperature */
	dev_ctx.temp_attrs.measure_value = ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.temp_attrs.min_measure_value = ENVIRONMENTAL_SENSOR_ATTR_TEMP_MIN;
	dev_ctx.temp_attrs.max_measure_value = ENVIRONMENTAL_SENSOR_ATTR_TEMP_MAX;
	dev_ctx.temp_attrs.tolerance = ENVIRONMENTAL_SENSOR_ATTR_TEMP_TOLERANCE;

	/* Humidity */
	dev_ctx.humidity_attrs.measure_value = ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.humidity_attrs.min_measure_value = ENVIRONMENTAL_SENSOR_ATTR_HUMIDITY_MIN;
	dev_ctx.humidity_attrs.max_measure_value = ENVIRONMENTAL_SENSOR_ATTR_HUMIDITY_MAX;
}

static void start_identifying(zb_bufid_t bufid)
{
	ZVUNUSED(bufid);

	if (ZB_JOINED()) {
		/*
		 * Check if endpoint is in identifying mode,
		 * if not put desired endpoint in identifying mode.
		 */
		if (dev_ctx.identify_attr.identify_time ==
		    ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE) {

			zb_ret_t zb_err_code =
				zb_bdb_finding_binding_target(ENVIRONMENTAL_SENSOR_ENDPOINT_NB);

			if (zb_err_code == RET_OK) {
				LOG_DBG("Manually enter identify mode");
			} else if (zb_err_code == RET_INVALID_STATE) {
				LOG_WRN("RET_INVALID_STATE - Cannot enter identify mode");
			} else {
				ZB_ERROR_CHECK(zb_err_code);
			}
		} else {
			LOG_DBG("Manually cancel identify mode");
			zb_bdb_finding_binding_target_cancel();
		}
	} else {
		LOG_WRN("Device not in a network - cannot identify itself");
	}
}

void zigbee_svc_configure_long_poll_interval(void)
{
	/* Ensure the device has joined the network before setting the poll interval */
	if (ZB_JOINED()) {
		zb_zdo_pim_set_long_poll_interval(LONG_POLL_PERIOD_MSEC);
		LOG_DBG("Long Poll interval set to %d ms", LONG_POLL_PERIOD_MSEC);
	} else {
		LOG_WRN("Device is not joined to the network. Cannot set Long Poll interval");
	}
}

/**@brief Function to toggle the identify LED
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void toggle_identify_led(zb_bufid_t bufid)
{
	int ret = ui_toggle_status_led();
	if (ret != 0) {
		LOG_ERR("Failed to toggle status LED: %d", ret);
	}
	ZB_SCHEDULE_APP_ALARM(toggle_identify_led, bufid, ZB_MILLISECONDS_TO_BEACON_INTERVAL(100));
}

/**@brief Function to handle identify notification events on the first endpoint.
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void identify_callback(zb_bufid_t bufid)
{
	zb_ret_t zb_err_code;

	if (bufid) {
		/* Schedule a self-scheduling function that will toggle the LED */
		ZB_SCHEDULE_APP_CALLBACK(toggle_identify_led, bufid);
	} else {
		/* Cancel the toggling function alarm and turn off LED */
		zb_err_code = ZB_SCHEDULE_APP_ALARM_CANCEL(toggle_identify_led, ZB_ALARM_ANY_PARAM);
		ZVUNUSED(zb_err_code);

		int ret = ui_set_status_led_off();
		if (ret != 0) {
			LOG_ERR("Failed to turn off status LED: %d", ret);
		}
	}
}

static void start_measuring(zb_bufid_t bufid)
{
	ZVUNUSED(bufid);

	int ret = humidity_temperature_svc_trigger_measurement();

	if (ret) {
		LOG_ERR("Failed to trigger humidity and temperature measurement: %d", ret);
	} else {
		ret = humidity_temperature_svc_update_zb_attributes();
		if (ret) {
			LOG_ERR("Failed to update humidity and temperature attributes: %d", ret);
		}
	}

	zb_ret_t zb_err = ZB_SCHEDULE_APP_ALARM(
		start_measuring, 0, ZB_MILLISECONDS_TO_BEACON_INTERVAL(MEASUREMENT_PERIOD_MSEC));
	if (zb_err) {
		LOG_ERR("Failed to schedule app alarm: %d", zb_err);
	}
}

void zboss_signal_handler(zb_bufid_t bufid)
{
	zb_zdo_app_signal_hdr_t *signal_header = NULL;
	zb_zdo_app_signal_type_t signal = zb_get_app_signal(bufid, &signal_header);
	zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);

	switch (signal) {
	case ZB_BDB_SIGNAL_DEVICE_REBOOT:
		/* fall-through */
	case ZB_BDB_SIGNAL_STEERING:
		/* Call default signal handler. */
		ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
		if (status == RET_OK) {
			/* Schedule first measurement */
			status = ZB_SCHEDULE_APP_ALARM(
				start_measuring, 0,
				ZB_MILLISECONDS_TO_BEACON_INTERVAL(FIRST_MEASUREMENT_DELAY_MSEC));
			if (status) {
				LOG_ERR("Failed to schedule app alarm: %d", status);
			}

			zigbee_svc_configure_long_poll_interval();
		}
		break;

	default:
		/* Let default signal handler process the signal*/
		ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
		break;
	}

	/*
	 * All callbacks should either reuse or free passed buffers.
	 * If bufid == 0, the buffer is invalid (not passed).
	 */
	if (bufid) {
		zb_buf_free(bufid);
	}
}

void zigbee_svc_start(void)
{
	/* Enable Sleepy End Device behavior */
	zb_set_rx_on_when_idle(ZB_FALSE);
	if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY)) {
		power_down_unused_ram();
	}

	zb_set_ed_timeout(CONFIG_NWK_ED_DEVICE_TIMEOUT_INDEX);
	zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(KEEP_ALIVE_PERIOD_MSEC));

	/* Start Zigbee stack */
	zigbee_enable();

	LOG_INF("Zigbee environmental sensor started");
}

void btn_callback(enum button_evt evt)
{
	if (evt == BUTTON_EVT_RELEASED_BEFORE_5_SEC) {
		/* Start identification mode */
		zb_ret_t ret = ZB_SCHEDULE_APP_CALLBACK(start_identifying, 0);
		if (ret) {
			LOG_ERR("Failed to schedule app callback: %d", ret);
		}

		/* Inform default signal handler about user input at the device */
		user_input_indicate();
	}

	if (evt == BUTTON_EVT_RELEASED_AFTER_5_SEC) {
		ZB_SCHEDULE_APP_CALLBACK(zb_bdb_reset_via_local_action, 0);
	}
}

void zigbee_svc_init(void)
{
	if (ui_gpio_init() != 0) {
		LOG_ERR("Failed to initialize GPIOs!");
	}

	ui_register_button_callback(btn_callback);

	/* Register device context (endpoint) */
	ZB_AF_REGISTER_DEVICE_CTX(&environmental_sensor_ctx);

	/* Init Basic and Identify and measurements-related attributes */
	zigbee_svc_clusters_init();

	/* Register callback to identify notifications */
	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(ENVIRONMENTAL_SENSOR_ENDPOINT_NB,
						identify_callback);
}
