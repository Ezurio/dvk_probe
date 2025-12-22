/*
 * Copyright 2025 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_led, CONFIG_DVK_PROBE_LOG_LEVEL);

#include "led.h"

const struct device *const led_strip = DEVICE_DT_GET(DT_ALIAS(ledstrip0));

ZBUS_CHAN_DEFINE(led_chan,                /* Name */
		 led_action_t,            /* Message type */
		 NULL,                    /* Validator */
		 NULL,                    /* User data */
		 ZBUS_OBSERVERS(led_sub), /* observers */
		 ZBUS_MSG_INIT(.dev = NULL, .color = LED_OFF, .on_time_ms = 0, .off_time_ms = 0,
			       .repeat_count = 0) /* Initial value */
);

const led_action_t LED_BLUE_FLASH = {
	.dev = led_strip,
	.color = LED_BLUE,
	.on_time_ms = LED_FLASH_FAST_TIME_MS,
	.off_time_ms = 0,
	.repeat_count = 0,
};

const led_action_t LED_RED_FLASH = {
	.dev = led_strip,
	.color = LED_RED,
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

int led_do_action(led_action_t *action)
{
	int ret;
	int repeat = 0;
	struct led_rgb off_color = LED_OFF;

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
		ret = led_strip_update_rgb(action->dev, &action->color, LED_STRIP_NUM_PIXELS);
		if (ret) {
			LOG_ERR("Failed to set LED color %d", ret);
			return ret;
		}
		k_msleep(action->on_time_ms);

		/* Turn off LED */
		ret = led_strip_update_rgb(action->dev, &off_color, LED_STRIP_NUM_PIXELS);
		if (ret) {
			LOG_ERR("Failed to turn off LED %d", ret);
			return ret;
		}
		k_msleep(action->off_time_ms);

		repeat++;
	} while (repeat < action->repeat_count);
	return 0;
}
