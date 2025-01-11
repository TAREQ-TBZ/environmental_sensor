/*
 * Copyright (c) 2024 Tareq Mhisen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "user_interface.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(user_interface, LOG_LEVEL_DBG);

static const struct gpio_dt_spec user_button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static struct gpio_callback user_button_cb_data;
static void (*button_callback)(enum button_evt evt) = NULL;
static atomic_t btn_evt;

static void timer_expiry_fn(struct k_timer *timer)
{
	int val = gpio_pin_get_dt(&user_button);
	if (val < 0) {
		LOG_ERR("Failed to read user button state: %d", val);
		return;
	}
	if (val == 1) {
		atomic_set(&btn_evt, BUTTON_EVT_PRESSED_MORE_THAN_5_SEC);
	}
}
static K_TIMER_DEFINE(btn_timer, timer_expiry_fn, NULL);

static void debouncing_finished(struct k_work *work)
{
	ARG_UNUSED(work);
	if (button_callback) {
		/* 1 = pressed, 0 = released */
		int val = gpio_pin_get_dt(&user_button);
		if (val < 0) {
			LOG_ERR("Failed to read user button state: %d", val);
			return;
		}
		if (val) {
			atomic_set(&btn_evt, BUTTON_EVT_PRESSED_LESS_THAN_5_SEC);
			k_timer_start(&btn_timer, K_SECONDS(5), K_NO_WAIT);
		} else {
			k_timer_stop(&btn_timer); /* Has no effect if the timer was not running */
			if (atomic_get(&btn_evt) == BUTTON_EVT_PRESSED_MORE_THAN_5_SEC) {
				atomic_set(&btn_evt, BUTTON_EVT_RELEASED_AFTER_5_SEC);
			} else {
				atomic_set(&btn_evt, BUTTON_EVT_RELEASED_BEFORE_5_SEC);
			}
			button_callback(atomic_get(&btn_evt));
		}
	} else {
		LOG_WRN("No registered user button callback!");
	}
}
static K_WORK_DELAYABLE_DEFINE(debouncing_work, debouncing_finished);

static void button_pressed_callback(const struct device *dev, struct gpio_callback *cb,
				    uint32_t pins)
{
	/* Debounce the button */
	k_work_reschedule(&debouncing_work, K_MSEC(15));
}

void ui_register_button_callback(void (*callback)(enum button_evt evt))
{
	button_callback = callback;
}

int ui_toggle_status_led(void)
{
	return gpio_pin_toggle_dt(&status_led);
}

int ui_set_status_led_on(void)
{
	return gpio_pin_set_dt(&status_led, 1);
}

int ui_set_status_led_off(void)
{
	return gpio_pin_set_dt(&status_led, 0);
}

int ui_gpio_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&status_led)) {
		LOG_ERR("Status LED device not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&user_button)) {
		LOG_ERR("User button device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE | GPIO_PULL_UP);
	if (ret < 0) {
		LOG_ERR("Failed to configure status LED: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&user_button, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0) {
		LOG_ERR("Failed to configure user button: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&user_button, GPIO_INT_EDGE_BOTH);
	if (ret < 0) {
		LOG_ERR("Failed to configure button interrupt: %d", ret);
		return ret;
	}

	gpio_init_callback(&user_button_cb_data, button_pressed_callback, BIT(user_button.pin));

	ret = gpio_add_callback(user_button.port, &user_button_cb_data);
	if (ret < 0) {
		LOG_ERR("Failed to add button callback: %d", ret);
		return ret;
	}

	atomic_set(&btn_evt, BUTTON_EVT_NONE);
	LOG_INF("GPIOs initialized successfully");

	return 0;
}
