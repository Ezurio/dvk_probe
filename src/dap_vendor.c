/*
 * @file dap_vendor.c
 *
 * Copyright 2026 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

/**************************************************************************************************/
/* Includes                                                                                       */
/**************************************************************************************************/
#include <zephyr/logging/log.h>
#include "dap_vendor.h"

LOG_MODULE_REGISTER(dap_vendor, LOG_LEVEL_INF);

/**************************************************************************************************/
/* Global Function Definitions                                                                    */
/**************************************************************************************************/
uint16_t dap_vendor_cmd_handler(uint8_t cmd_id, const uint8_t *request, uint8_t *response)
{
	uint16_t response_len = 0;

	switch (cmd_id) {
	default:
		LOG_WRN("Unknown vendor command: 0x%02X", cmd_id);
		/* Unknown vendor command */
		response[0] = ID_DAP_INVALID;
		response_len = 1;
		break;
	}
	return response_len;
}
