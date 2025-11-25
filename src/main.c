/*
 * Copyright 2025 Google LLC
 * Copyright 2025 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

#include "app_usbd.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart/uart_bridge.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <cmsis_dap.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dvk_probe, LOG_LEVEL_INF);

static struct usbd_context *app_usbd;

#define DEVICE_DT_GET_COMMA(node_id) DEVICE_DT_GET(node_id),

const struct device *uart_bridges[] = {
	DT_FOREACH_STATUS_OKAY(zephyr_uart_bridge, DEVICE_DT_GET_COMMA)};

static void usbd_msg_cb(struct usbd_context *const ctx, const struct usbd_msg *msg)
{
	uint32_t line_ctrl_status;
	uint8_t dev_idx;
	const struct device *uart_dev = NULL;
	int ret;
	struct uart_config peer_cfg;

	LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

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
	const struct device *const swd_dev = DEVICE_DT_GET_ONE(zephyr_swdp_gpio);

	err = dap_setup(swd_dev);
	if (err) {
		LOG_ERR("Failed to initialize DAP controller, %d", err);
		return err;
	}

	app_usbd = app_usbd_init_device(usbd_msg_cb);
	if (app_usbd == NULL) {
		LOG_ERR("Failed to initialize USB device");
		return -ENODEV;
	}

	if (!usbd_can_detect_vbus(app_usbd)) {
		err = usbd_enable(app_usbd);
		if (err) {
			LOG_ERR("Failed to enable device support");
			return err;
		}
	}

	LOG_INF("USB device support enabled");

	k_sleep(K_FOREVER);

	return 0;
}
