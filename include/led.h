/*
 * Copyright 2025 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

#ifndef APP_LED_H
#define APP_LED_H

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/zbus/zbus.h>

#if DT_NODE_HAS_PROP(DT_ALIAS(ledstrip0), chain_length)
#define LED_STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(ledstrip0), chain_length)
#else
#error Unable to determine length of LED strip
#endif

#define LED_LEVEL_LIMIT 0.25 /* Limit max brightness to 25% to protect eyes */

#define LED_RGB(_r, _g, _b)                                                                        \
	{.r = ((uint8_t)((_r) * LED_LEVEL_LIMIT)),                                                 \
	 .g = ((uint8_t)((_g) * LED_LEVEL_LIMIT)),                                                 \
	 .b = ((uint8_t)((_b) * LED_LEVEL_LIMIT))}

#define LED_COLOR_RED   LED_RGB(255, 0, 0)
#define LED_COLOR_GREEN LED_RGB(0, 255, 0)
#define LED_COLOR_BLUE  LED_RGB(0, 0, 255)
#define LED_COLOR_WHITE  LED_RGB(255, 255, 255)
#define LED_COLOR_OFF   LED_RGB(0, 0, 0)

#define LED_FLASH_TIME_MS      30
#define LED_FLASH_FAST_TIME_MS 10

typedef struct led_action {
	const struct device *dev;
	struct led_rgb color;
	uint16_t on_time_ms;
	uint16_t off_time_ms;
	uint16_t repeat_count;
} led_action_t;

typedef enum {
	LED_BLACK, /* OFF */
	LED_RED,
	LED_GREEN,
	LED_BLUE,
	LED_WHITE,
	NUMBER_OF_LED_COLORS,
} led_color_t;

extern const struct device *const led_strip;
extern const led_action_t LED_BLUE_FLASH;
extern const led_action_t LED_RED_FLASH;
extern const led_action_t LED_GREEN_FLASH;

/**
 * @brief Send LED action to LED handler
 *
 * @param action An action to be performed on the LED. This queues the action for processing.
 *		The LED action is designed to flash LEDs and is not suitable for setting steady
 *		states.
 * @return int 0 on success, negative error code on failure
 */
int led_send_action(led_action_t *action);

/**
 * @brief Perform LED action
 *
 * @param action An action to be performed on the LED.
 *		This is a blocking call. It cannot be called from an interrupt context.
 *		The LED action is designed to flash LEDs and is not suitable for setting steady
 *		states.
 * @return int 0 on success, negative error code on failure
 */
int led_do_action(led_action_t *action);

/**
 * @brief Toggle LED color channels
 *
 * @param led_color Color channel to toggle.
 */
void toggle_led(led_color_t led_color);

/**
 * @brief Turn off specified LED color channel
 *
 * @param led_color Color channel to turn off.
 */
void led_off(led_color_t led_color);

/**
 * @brief Initialize LED subsystem
 */
int led_init(void);

#endif /* APP_LED_H */
