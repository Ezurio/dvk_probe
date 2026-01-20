/**
 * @file probe_settings.c
 * @brief API for probe NV settings
 *
 * Copyright (c) 2026 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

/**************************************************************************************************/
/* Includes                                                                                       */
/**************************************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "probe_settings.h"

LOG_MODULE_REGISTER(probe_settings, LOG_LEVEL_INF);

/**************************************************************************************************/
/* Local Constant, Macro and Type Definitions                                                     */
/**************************************************************************************************/
#define DEFAULT_USB_VID CONFIG_APP_USBD_VID  /* Ezurio VID */
#define DEFAULT_USB_PID CONFIG_APP_USBD_PID  /* DVK Probe PID */
#define SETTINGS_PARTITION_ID FIXED_PARTITION_ID(settings_partition)
#define SETTINGS_PAGE_SIZE PROBE_SETTINGS_MAX_SIZE  /* 256 bytes */
#define SETTINGS_SECTOR_SIZE 4096  /* Total sector size */

/**************************************************************************************************/
/* Local Data Definitions                                                                         */
/**************************************************************************************************/
static probe_settings_ut probe_settings_data;

static const probe_settings_ut probe_settings_default = {
	.v2 = {
		.version = PROBE_SETTINGS_V2,
		.target_board_name = CONFIG_CMSIS_DAP_BOARD_NAME,
		.target_board_vendor = CONFIG_CMSIS_DAP_BOARD_VENDOR,
		.target_device_name = CONFIG_CMSIS_DAP_DEVICE_NAME,
		.target_device_vendor = CONFIG_CMSIS_DAP_DEVICE_VENDOR,
		.usb_vid = DEFAULT_USB_VID,
		.usb_pid = DEFAULT_USB_PID
	}
};

static const struct flash_area *settings_area;
static const struct device *flash_dev;
static uint32_t current_settings_offset;
static uint32_t next_settings_offset;

/**************************************************************************************************/
/* Local Function Definitions                                                                     */
/**************************************************************************************************/
static void find_internal_settings(void)
{
	uint32_t page_size = SETTINGS_PAGE_SIZE;
	probe_settings_ut settings;
	int ret;

	/* Initialize to start of partition */
	current_settings_offset = 0;
	next_settings_offset = 0;

	/* Search through the sector for valid settings */
	for (uint32_t offset = 0; offset < SETTINGS_SECTOR_SIZE; offset += page_size) {
		ret = flash_area_read(settings_area, offset, &settings, sizeof(probe_settings_ut));
		if (ret < 0) {
			continue;
		}

		if (settings.base.version != PROBE_SETTINGS_INVALID_FF &&
		    settings.base.version != PROBE_SETTINGS_INVALID_00) {
			/* Found valid settings */
			current_settings_offset = offset;
			/* Next write location is the following page, wrapping around */
			if (offset + page_size >= SETTINGS_SECTOR_SIZE) {
				next_settings_offset = 0;
			} else {
				next_settings_offset = offset + page_size;
			}
		}
	}
}

static int read_settings_from_flash(probe_settings_ut *settings)
{
	int ret;

	ret = flash_area_read(settings_area, current_settings_offset, settings, sizeof(probe_settings_ut));
	if (ret < 0) {
		LOG_ERR("Failed to read settings from flash: %d", ret);
		return ret;
	}

	return 0;
}

static void settings_v1_to_v2(probe_settings_ut *settings)
{
	settings->v2.version = PROBE_SETTINGS_V2;
	settings->v2.usb_vid = DEFAULT_USB_VID;
	settings->v2.usb_pid = DEFAULT_USB_PID;
}

/**************************************************************************************************/
/* Global Data Definitions                                                                        */
/**************************************************************************************************/
const probe_settings_ut *probe_settings = (const probe_settings_ut *)&probe_settings_data;

/**************************************************************************************************/
/* Global Function Definitions                                                                    */
/**************************************************************************************************/
void probe_settings_init(void)
{
	int ret;
	probe_settings_ut temp_settings;

	/* Fill settings data with zeroes to NULL all settings strings */
	memset(&probe_settings_data, 0, sizeof(probe_settings_ut));

	/* Open the settings partition */
	ret = flash_area_open(SETTINGS_PARTITION_ID, &settings_area);
	if (ret < 0) {
		LOG_ERR("Failed to open settings partition: %d", ret);
		return;
	}

	flash_dev = flash_area_get_device(settings_area);
	if (flash_dev == NULL) {
		LOG_ERR("Failed to get flash device");
		flash_area_close(settings_area);
		return;
	}

	/* Search for valid settings in the sector */
	find_internal_settings();

	/* Read current settings from flash */
	ret = read_settings_from_flash(&temp_settings);
	if (ret < 0) {
		/* Failed to read, use defaults */
		LOG_WRN("Using default settings");
		write_internal_settings(&probe_settings_default, PROBE_SETTINGS_MAX_SIZE);
		memcpy(&probe_settings_data, &probe_settings_default, sizeof(probe_settings_ut));
		return;
	}

	/* Check if settings are valid */
	if (temp_settings.base.version == PROBE_SETTINGS_INVALID_FF ||
	    temp_settings.base.version == PROBE_SETTINGS_INVALID_00) {
		/* Invalid settings, write defaults */
		LOG_INF("No valid settings found, writing defaults");
		write_internal_settings(&probe_settings_default, PROBE_SETTINGS_MAX_SIZE);
		memcpy(&probe_settings_data, &probe_settings_default, sizeof(probe_settings_ut));
	} else if (temp_settings.base.version == PROBE_SETTINGS_V1) {
		/* Upgrade from V1 to V2 */
		LOG_INF("Upgrading settings from V1 to V2");
		settings_v1_to_v2(&temp_settings);
		write_internal_settings(&temp_settings, PROBE_SETTINGS_MAX_SIZE);
		memcpy(&probe_settings_data, &temp_settings, sizeof(probe_settings_ut));
	} else {
		/* Valid settings, use them */
		LOG_INF("Loaded settings version %d", temp_settings.base.version);
		memcpy(&probe_settings_data, &temp_settings, sizeof(probe_settings_ut));
	}
}

int write_internal_settings(const probe_settings_ut *settings, uint16_t size)
{
	int ret = 0;
	probe_settings_ut verify_settings;

	if (settings == NULL) {
		return -SETTINGS_ERR_INVALID_PARAM;
	}

	if (size > PROBE_SETTINGS_MAX_SIZE) {
		return -SETTINGS_ERR_INVALID_PARAM;
	}

	if (settings_area == NULL) {
		LOG_ERR("Settings partition not opened");
		return -ENODEV;
	}

	/* Check if next write location is empty (0xFF) or needs erase */
	ret = flash_area_read(settings_area, next_settings_offset, &verify_settings, sizeof(probe_settings_ut));
	if (ret < 0) {
		LOG_ERR("Failed to read next settings location: %d", ret);
		return ret;
	}

	/* If not empty, erase the entire sector */
	if (verify_settings.base.version != PROBE_SETTINGS_INVALID_FF) {
		LOG_INF("Erasing settings sector");
		ret = flash_area_erase(settings_area, 0, settings_area->fa_size);
		if (ret < 0) {
			LOG_ERR("Failed to erase settings partition: %d", ret);
			return ret;
		}
		/* After erase, reset to beginning of sector */
		next_settings_offset = 0;
	}

	/* Write new settings at next offset */
	ret = flash_area_write(settings_area, next_settings_offset, settings, size);
	if (ret < 0) {
		LOG_ERR("Failed to write settings: %d", ret);
		return ret;
	}

	/* Update page tracking */
	find_internal_settings();

	/* Update cached settings */
	memcpy(&probe_settings_data, settings, sizeof(probe_settings_ut));

	LOG_INF("Settings written successfully at offset %u", next_settings_offset);
	return 0;
}
