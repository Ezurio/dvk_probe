/*
 * Copyright 2025 Google LLC
 * Copyright 2025 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/app_version.h>
#include <cmsis_dap.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dvk_probe, CONFIG_DVK_PROBE_LOG_LEVEL);

#include "uart_bridge.h"
#include "app_usbd.h"
#include "led.h"
#include "probe_settings.h"
#include "dap_vendor.h"

static struct usbd_context *app_usbd;

#define DEVICE_DT_GET_COMMA(node_id) DEVICE_DT_GET(node_id),

static const struct device *const swd_dev = DEVICE_DT_GET_ONE(zephyr_swdp_gpio);

static const struct device *uart_bridges[] = {
	DT_FOREACH_STATUS_OKAY(rfpros_uart_bridge, DEVICE_DT_GET_COMMA)};

// Get the node ID of gpio_dynamic
#define GPIO_DYNAMIC_NODE DT_PATH(gpio_dynamic)

// Macro to extract gpio_dt_spec from a child node
#define GPIO_SPEC_FROM_CHILD(child_node) GPIO_DT_SPEC_GET_BY_IDX(child_node, gpios, 0),

// Get all dynamic GPIOs
static const struct gpio_dt_spec gpios[] = {
	DT_FOREACH_CHILD(GPIO_DYNAMIC_NODE, GPIO_SPEC_FROM_CHILD)};

ZBUS_CHAN_DECLARE(led_chan);
ZBUS_SUBSCRIBER_DEFINE(led_sub, 8);

static void usbd_msg_cb(struct usbd_context *const ctx, const struct usbd_msg *msg)
{
	uint32_t line_ctrl_status;
	uint8_t dev_idx;
	const struct device *uart_dev = NULL;
	int ret;
	struct uart_config peer_cfg;

	LOG_DBG("USBD message: %s", usbd_msg_type_string(msg->type));

	if (usbd_can_detect_vbus(ctx)) {
		if (msg->type == USBD_MSG_VBUS_READY) {
			if (usbd_enable(ctx)) {
				LOG_ERR("Failed to enable device support");
			}
		}

		if (msg->type == USBD_MSG_VBUS_REMOVED) {
			if (usbd_disable(ctx)) {
				LOG_ERR("Failed to disable device support");
			}
		}
	}

	if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING ||
	    msg->type == USBD_MSG_CDC_ACM_CONTROL_LINE_STATE) {
		for (dev_idx = 0; dev_idx < ARRAY_SIZE(uart_bridges); dev_idx++) {
			/* Find the matching device */
			uart_dev = uart_bridge_get_peer(msg->dev, uart_bridges[dev_idx]);
			if (uart_dev != NULL) {
				break;
			}
		}

		if (uart_dev == NULL) {
			LOG_DBG("No matching UART bridge for %s", msg->dev->name);
			return;
		}

		/* Get the current UART configuration of the USB CDC ACM device */
		ret = uart_config_get(msg->dev, &peer_cfg);
		if (ret != 0) {
			LOG_ERR("Failed to get UART config: %d", ret);
			return;
		}

		/* Check if the USB CDC ACM device is ready for operation */
		ret = uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_DTR, &line_ctrl_status);
		if (ret == 0) {
			if (line_ctrl_status) {
				LOG_INF("DTR set: enable UART bridge %s", uart_dev->name);
				uart_bridge_settings_update(msg->dev, uart_bridges[dev_idx]);
				peer_cfg.flow_ctrl = UART_CFG_FLOW_CTRL_RTS_CTS;
			} else {
				LOG_INF("DTR cleared: disable UART bridge %s", uart_dev->name);
				/* This sets RTS back high when the USB UART is closed */
				peer_cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
			}

			ret = uart_configure(uart_dev, &peer_cfg);
			if (ret) {
				LOG_ERR("%s: failed to set the uart config: %d", uart_dev->name,
					ret);
				return;
			}
		} else {
			LOG_ERR("Failed to get DTR status: %d", ret);
		}
	}
}

int main(void)
{
	int err;
	led_action_t led_boot_action = {
		.dev = led_strip,
		.color = LED_COLOR_WHITE,
		.on_time_ms = LED_FLASH_TIME_MS,
		.off_time_ms = LED_FLASH_TIME_MS,
		.repeat_count = 2,
	};
	led_action_t msg_led_action;
	const struct zbus_channel *chan;

	for (size_t i = 0; i < ARRAY_SIZE(gpios); i++) {
		if (gpio_pin_configure_dt(&gpios[i], GPIO_DISCONNECTED) != 0) {
			LOG_ERR("Could not configure GPIO %s", gpios[i].port->name);
		}
	}

	if (led_init() != 0) {
		LOG_ERR("LED strip device %s is not ready", led_strip->name);
		return -ENODEV;
	}

	err = dap_setup(swd_dev);
	if (err) {
		LOG_ERR("Failed to initialize DAP controller, %d", err);
		return err;
	}

	dap_set_fw_version(APP_VERSION_STRING);

	err = dap_set_vendor_cmd_callback(dap_vendor_cmd_handler);
	if (err < 0) {
		LOG_ERR("Failed to register vendor command callback: %d", err);
	}

	/* Initialize probe settings */
	probe_settings_init();

	/* Set DAP device info from probe settings */
	if (probe_settings != NULL && probe_settings->base.version >= PROBE_SETTINGS_V1) {
		err = dap_set_device_info(probe_settings->v1.target_device_vendor,
					  probe_settings->v1.target_device_name,
					  probe_settings->v1.target_board_vendor,
					  probe_settings->v1.target_board_name);
		if (err) {
			LOG_WRN("Failed to set DAP device info: %d", err);
		} else {
			LOG_INF("DAP device info set from probe settings");
		}
	}

	app_usbd = app_usbd_setup_device(usbd_msg_cb);
	if (app_usbd == NULL) {
		LOG_ERR("Failed to setup USB device");
		return -ENODEV;
	}

	/* Set USB VID and PID from probe settings */
	if (probe_settings != NULL && probe_settings->base.version >= PROBE_SETTINGS_V2) {
		err = usbd_device_set_vid(app_usbd, probe_settings->v2.usb_vid);
		if (err) {
			LOG_WRN("Failed to set USB VID: %d", err);
		}
		err = usbd_device_set_pid(app_usbd, probe_settings->v2.usb_pid);
		if (err) {
			LOG_WRN("Failed to set USB PID: %d", err);
		}
		LOG_INF("USB VID:PID set to 0x%04x:0x%04x", probe_settings->v2.usb_vid,
			probe_settings->v2.usb_pid);
	}

	/* Initialize USB device */
	err = usbd_init(app_usbd);
	if (err) {
		LOG_ERR("Failed to initialize USB device: %d", err);
		return err;
	}

	if (!usbd_can_detect_vbus(app_usbd)) {
		err = usbd_enable(app_usbd);
		if (err) {
			LOG_ERR("Failed to enable device support");
			return err;
		}
	}

	LOG_INF("USB device support enabled");

	/* Flash LED to indicate boot */
	led_send_action(&led_boot_action);

	/* Main loop to process LED actions */
	while (1) {
		err = zbus_sub_wait(&led_sub, &chan, K_FOREVER);
		if (err) {
			LOG_ERR("Failed to wait for LED action: %d", err);
			k_yield();
			continue;
		}
		if (chan != &led_chan) {
			k_yield();
			continue;
		}
		err = zbus_chan_read(&led_chan, &msg_led_action, K_FOREVER);
		if (err == 0) {
			led_do_action(&msg_led_action);
		}
		k_sleep(K_MSEC(1));
		k_yield();
	}
	return 0;
}
