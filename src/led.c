/*
 * Copyright 2025 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_led, CONFIG_DVK_PROBE_LOG_LEVEL);

#include "led.h"

const struct device *const led_strip = DEVICE_DT_GET(DT_ALIAS(ledstrip0));
/* Current LED status */
static struct led_rgb led_status;
/* New LED status to be applied */
static struct led_rgb new_led_status;
static struct k_work led_update_work;
static struct k_spinlock led_status_lock;

ZBUS_CHAN_DEFINE(led_chan,                /* Name */
		 led_action_t,            /* Message type */
		 NULL,                    /* Validator */
		 NULL,                    /* User data */
		 ZBUS_OBSERVERS(led_sub), /* observers */
		 ZBUS_MSG_INIT(.dev = NULL, .color = LED_COLOR_OFF, .on_time_ms = 0,
			       .off_time_ms = 0, .repeat_count = 0) /* Initial value */
);

const led_action_t LED_BLUE_FLASH = {
	.dev = led_strip,
	.color = LED_COLOR_BLUE,
	.on_time_ms = LED_FLASH_FAST_TIME_MS,
	.off_time_ms = 0,
	.repeat_count = 0,
};

const led_action_t LED_RED_FLASH = {
	.dev = led_strip,
	.color = LED_COLOR_RED,
	.on_time_ms = LED_FLASH_FAST_TIME_MS,
	.off_time_ms = 0,
	.repeat_count = 0,
};

const led_action_t LED_GREEN_FLASH = {
	.dev = led_strip,
	.color = LED_COLOR_GREEN,
	.on_time_ms = LED_FLASH_FAST_TIME_MS,
	.off_time_ms = 0,
	.repeat_count = 0,
};

int led_send_action(led_action_t *action)
{
	int ret;

	if (action == NULL) {
		return -EINVAL;
	}

	ret = zbus_chan_pub(&led_chan, action, K_NO_WAIT);
	if (ret == 0) {
		LOG_DBG("Published LED Action: Color R:%d G:%d B:%d On:%d Off:%d Repeat:%d",
			action->color.r, action->color.g, action->color.b, action->on_time_ms,
			action->off_time_ms, action->repeat_count);
	}

	return ret;
}

static int set_led_color(const struct device *dev, struct led_rgb *led_color)
{
	int ret;
	k_spinlock_key_t key;

	ret = led_strip_update_rgb(dev, led_color, LED_STRIP_NUM_PIXELS);
	if (ret) {
		LOG_ERR("Failed to set LED color %d", ret);
		return ret;
	}

	key = k_spin_lock(&led_status_lock);
	memcpy(&led_status, led_color, sizeof(struct led_rgb));
	k_spin_unlock(&led_status_lock, key);

	return 0;
}

int led_do_action(led_action_t *action)
{
	int ret;
	int repeat = 0;
	struct led_rgb off_color = LED_COLOR_OFF;

	if (action == NULL) {
		return -EINVAL;
	}

	if (action->dev == NULL) {
		return -EINVAL;
	}

	LOG_DBG("Run LED Action: Color R:%d G:%d B:%d On:%d Off:%d Repeat:%d", action->color.r,
		action->color.g, action->color.b, action->on_time_ms, action->off_time_ms,
		action->repeat_count);

	do {
		/* Set LED to desired color */
		ret = set_led_color(action->dev, &action->color);
		if (ret) {
			LOG_ERR("Failed to set LED color %d", ret);
			return ret;
		}
		k_msleep(action->on_time_ms);

		/* Turn off LED */
		ret = set_led_color(action->dev, &off_color);
		if (ret) {
			LOG_ERR("Failed to turn off LED %d", ret);
			return ret;
		}
		k_msleep(action->off_time_ms);

		repeat++;
	} while (repeat < action->repeat_count);
	return 0;
}

static void led_update_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	set_led_color(led_strip, &new_led_status);
}

void toggle_led(led_color_t led_color)
{
	k_spinlock_key_t key;

	key = k_spin_lock(&led_status_lock);
	switch (led_color) {
	case LED_RED:
		new_led_status.r = led_status.r ? 0 : (int)(255 * LED_LEVEL_LIMIT);
		break;
	case LED_GREEN:
		new_led_status.g = led_status.g ? 0 : (int)(255 * LED_LEVEL_LIMIT);
		break;
	case LED_BLUE:
		new_led_status.b = led_status.b ? 0 : (int)(255 * LED_LEVEL_LIMIT);
		break;
	default:
		return;
	}
	k_spin_unlock(&led_status_lock, key);
	k_work_submit(&led_update_work);
}

void led_off(led_color_t led_color)
{
	k_spinlock_key_t key;

	key = k_spin_lock(&led_status_lock);
	switch (led_color) {
	case LED_RED:
		new_led_status.r = 0;
		break;
	case LED_GREEN:
		new_led_status.g = 0;
		break;
	case LED_BLUE:
		new_led_status.b = 0;
		break;
	default:
		return;
	}
	k_spin_unlock(&led_status_lock, key);
	k_work_submit(&led_update_work);
}

int led_init(void)
{
	if (!device_is_ready(led_strip)) {
		LOG_ERR("LED strip device not ready");
		return -ENODEV;
	}

	k_work_init(&led_update_work, led_update_work_handler);

	/* Initialize LED strip to off */
	set_led_color(led_strip, &led_status);
	return 0;
}
