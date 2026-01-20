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
#include <pico/stdlib.h>
#include <string.h>
#include <hardware/sync.h>
#include <hardware/flash.h>
#include "picoprobe_config.h"
#include "board_id.h"
#include "probe_settings.h"

/**************************************************************************************************/
/* Local Constant, Macro and Type Definitions                                                     */
/**************************************************************************************************/
#define BOARD_ID_ADDRESS_START 0

#define SETTINGS_INTERNAL_START (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

/**************************************************************************************************/
/* Local Data Definitions                                                                         */
/**************************************************************************************************/
static probe_settings_ut probe_settings_data;

static const probe_settings_v2_t probe_settings_default = {.version = PROBE_SETTINGS_V2,
							   .target_board_name = "Module",
							   .target_board_vendor =
								   PICOPROBE_VENDOR_STRING,
							   .target_device_name = "cortex_m",
							   .target_device_vendor = "Arm",
							   .usb_vid = DEFAULT_USB_VID,
							   .usb_pid = DEFAULT_USB_PID};

static probe_settings_ut *current_settings_page;
static probe_settings_ut *next_settings_page;

/**************************************************************************************************/
/* Local Function Prototypes                                                                      */
/**************************************************************************************************/
static void find_internal_settings(void);

/**************************************************************************************************/
/* Local Function Definitions                                                                     */
/**************************************************************************************************/
static void find_internal_settings(void)
{
	uint32_t settings_start = SETTINGS_INTERNAL_START;
	uint32_t settings_end = PICO_FLASH_SIZE_BYTES;
	uint32_t page_size = FLASH_PAGE_SIZE;
	probe_settings_ut *settings;

	for (uint32_t i = settings_start; i < settings_end; i += page_size) {
		settings = (probe_settings_ut *)(XIP_BASE + i);
		if (settings->base.version != PROBE_SETTINGS_INVALID_FF &&
		    settings->base.version != PROBE_SETTINGS_INVALID_00) {
			current_settings_page = settings;
			if (i + page_size >= settings_end) {
				next_settings_page =
					(probe_settings_ut *)(XIP_BASE + SETTINGS_INTERNAL_START);
			} else {
				next_settings_page =
					(probe_settings_ut *)(XIP_BASE + i + page_size);
			}
		}
	}
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
	bool board_id_present = true;

	/* Fill settings data with zeroes to NULL all settings strings */
	memset(&probe_settings_data, 0, sizeof(probe_settings_ut));
	current_settings_page = next_settings_page =
		(probe_settings_ut *)(XIP_BASE + SETTINGS_INTERNAL_START);

	find_internal_settings();

	if (current_settings_page->base.version == PROBE_SETTINGS_INVALID_FF ||
	    current_settings_page->base.version == PROBE_SETTINGS_INVALID_00) {
		ret = board_id_read_bytes(BOARD_ID_ADDRESS_START, (uint8_t *)&probe_settings_data,
					  sizeof(probe_settings_data));

		if (ret != sizeof(probe_settings_data)) {
			board_id_present = false;
		}

		if (board_id_present && probe_settings->base.version != PROBE_SETTINGS_INVALID_FF &&
		    board_id_present && probe_settings->base.version != PROBE_SETTINGS_INVALID_00) {
			settings_v1_to_v2((probe_settings_ut *)probe_settings);
			write_internal_settings(probe_settings, PROBE_SETTINGS_MAX_SIZE);
		} else {
			write_internal_settings((const probe_settings_ut *)&probe_settings_default, PROBE_SETTINGS_MAX_SIZE);
		}
	} else if (current_settings_page->base.version == PROBE_SETTINGS_V1) {
		settings_v1_to_v2(current_settings_page);
		write_internal_settings(current_settings_page, PROBE_SETTINGS_MAX_SIZE);
	}
	memcpy(&probe_settings_data, current_settings_page, sizeof(probe_settings_ut));
}

int write_internal_settings(const probe_settings_ut *settings, uint16_t size)
{
	int ret = 0;
	uint32_t ints, flash_offs;

	if (settings == NULL) {
		return -SETTINGS_ERR_INVALID_PARAM;
	}

	if (size > FLASH_PAGE_SIZE) {
		return -SETTINGS_ERR_INVALID_PARAM;
	}

	if (next_settings_page->base.version != PROBE_SETTINGS_INVALID_FF) {
		ints = save_and_disable_interrupts();
		flash_range_erase(SETTINGS_INTERNAL_START, FLASH_SECTOR_SIZE);
		restore_interrupts(ints);
	}

	flash_offs = (uint32_t)((uint32_t)next_settings_page - XIP_BASE);
	ints = save_and_disable_interrupts();
	flash_range_program(flash_offs, (uint8_t *)settings, size);
	restore_interrupts(ints);

	find_internal_settings();
	memcpy(&probe_settings_data, current_settings_page, sizeof(probe_settings_ut));

	return ret;
}
