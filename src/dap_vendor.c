/*
 * @file dap_vendor.c
 *
 * Copyright 2026 Ezurio
 * Copyright (c) 2023 Laird Connectivity LLC
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

/**************************************************************************************************/
/* Includes                                                                                       */
/**************************************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>
#include <pico/bootrom.h>
#include <string.h>
#include "dap_vendor.h"
#include "probe_settings.h"

LOG_MODULE_REGISTER(dap_vendor, LOG_LEVEL_INF);

/**************************************************************************************************/
/* Local Constant, Macro and Type Definitions                                                     */
/**************************************************************************************************/
#define REBOOT_DELAY_MS 100

// Get the node ID of gpio_dynamic
#define GPIO_DYNAMIC_NODE DT_PATH(gpio_dynamic)

// Macro to extract gpio_dt_spec from a child node
#define GPIO_SPEC_FROM_CHILD(child_node) GPIO_DT_SPEC_GET_BY_IDX(child_node, gpios, 0),

/**************************************************************************************************/
/* Local Data Definitions                                                                         */
/**************************************************************************************************/
// Get all dynamic GPIOs
static const struct gpio_dt_spec gpios[] = {
	DT_FOREACH_CHILD(GPIO_DYNAMIC_NODE, GPIO_SPEC_FROM_CHILD)};

static struct k_work_delayable reboot_work;
static struct k_work_delayable reboot_bootloader_work;

/**************************************************************************************************/
/* Local Function Definitions                                                                     */
/**************************************************************************************************/
static void reboot_work_handler(struct k_work *work)
{
	/* Reboot the system */
	sys_reboot(SYS_REBOOT_COLD);
}

static void reboot_bootloader_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	/* Reboot to the bootloader. This call never returns. */
	reset_usb_boot(0, 0);
}

static int convert_io(uint8_t io)
{
	switch (io) {
	case 16:
		return 0;
	case 17:
		return 1;
	case 18:
		return 2;
	case 19:
		return 3;
	case 20:
		return 4;
	case 21:
		return 5;
	case 25:
		return 6;
	case 26:
		return 7;
	case 27:
		return 8;
	case 28:
		return 9;
	default:
		return -1;
	}
}

static int check_io_range(uint8_t io)
{
	int gpio = convert_io(io);
	if (gpio < 0) {
		return -DAP_VENDOR_ERR_INVALID_IO;
	}

	return 0;
}

static int set_io_dir(uint8_t gpio, uint8_t dir, uint8_t option)
{
	int ret = 0;
	gpio_flags_t flags = 0;
	uint8_t io;

	ret = check_io_range(gpio);
	if (ret != 0) {
		return ret;
	}

	io = convert_io(gpio);

	if (option >= IO_OPTION_INVALID) {
		ret = -DAP_VENDOR_ERR_INVALID_IO_OPTION;
		return ret;
	}

	if (option == IO_OPTION_DISCONNECT) {
		/* Disconnect the pin */
		ret = gpio_pin_configure_dt(&gpios[io], GPIO_DISCONNECTED);
		return ret;
	}

	/* Set direction: input or output */
	if (dir) {
		flags = GPIO_OUTPUT;
	} else {
		flags = GPIO_INPUT;

		/* Configure pull resistors for inputs */
		if (option == IO_OPTION_PULL_DOWN) {
			flags |= GPIO_PULL_DOWN;
		} else if (option == IO_OPTION_PULL_UP) {
			flags |= GPIO_PULL_UP;
		}
	}

	ret = gpio_pin_configure_dt(&gpios[io], flags);
	return ret;
}

static int set_io(uint8_t gpio, uint8_t level)
{
	int ret = 0;
	uint8_t io;

	ret = check_io_range(gpio);
	if (ret != 0) {
		return ret;
	}

	io = convert_io(gpio);

	ret = gpio_pin_set_raw(gpios[io].port, gpios[io].pin, level);
	return ret;
}

static int read_io(uint8_t gpio)
{
	int ret = 0;
	uint8_t io;

	ret = check_io_range(gpio);
	if (ret != 0) {
		return ret;
	}

	io = convert_io(gpio);

	ret = gpio_pin_get_raw(gpios[io].port, gpios[io].pin);
	return ret;
}

/**************************************************************************************************/
/* Global Function Definitions                                                                    */
/**************************************************************************************************/
uint16_t dap_vendor_cmd_handler(uint8_t cmd_id, const uint8_t *request, uint8_t *response)
{
	int ret;
	int temp;
	uint16_t response_len = 2;

	/* First byte is always the command ID */
	response[0] = cmd_id;

	switch (cmd_id) {
	case ID_DAP_VENDOR_SET_IO_DIR:
		ret = set_io_dir(request[0], request[1], request[2]);
		response[1] = ret;
		break;

	case ID_DAP_VENDOR_SET_IO:
		ret = set_io(request[0], request[1]);
		response[1] = ret;
		break;

	case ID_DAP_VENDOR_READ_IO:
		ret = read_io(request[0]);
		response[1] = ret;
		break;

	case ID_DAP_VENDOR_REBOOT:
		if (request[0]) {
			k_work_init_delayable(&reboot_bootloader_work,
					      reboot_bootloader_work_handler);
			k_work_schedule(&reboot_bootloader_work, K_MSEC(REBOOT_DELAY_MS));
			response[1] = 0;
		} else {
			/* Schedule a delayed reboot so the USB response can be sent */
			k_work_init_delayable(&reboot_work, reboot_work_handler);
			k_work_schedule(&reboot_work, K_MSEC(REBOOT_DELAY_MS));
			response[1] = 0;
		}
		break;

	case ID_DAP_VENDOR_READ_SETTINGS:
		if (probe_settings != NULL) {
			memcpy(&response[1], probe_settings, PROBE_SETTINGS_MAX_SIZE);
			response_len = PROBE_SETTINGS_MAX_SIZE + 1;
		} else {
			response[1] = -1;
			response_len = 1;
		}
		break;

	case ID_DAP_VENDOR_WRITE_SETTINGS:
		temp = request[0];
		if (temp > PROBE_SETTINGS_MAX_SIZE) {
			ret = -DAP_VENDOR_ERR_INVALID_SIZE;
			response[1] = ret;
		} else {
			ret = write_internal_settings((const probe_settings_ut *)(request + 1),
						      (uint16_t)temp);
			response[1] = ret;
		}
		break;

	default:
		LOG_WRN("Unknown vendor command: 0x%02X", cmd_id);
		/* Unknown vendor command */
		response[0] = ID_DAP_INVALID;
		response_len = 1;
		break;
	}

	return response_len;
}
