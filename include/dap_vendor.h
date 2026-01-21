/**
 * @file dap_vendor.h
 *
 * Copyright (c) 2026 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

#ifndef __DAP_VENDOR_H__
#define __DAP_VENDOR_H__

/**************************************************************************************************/
/* Includes                                                                                       */
/**************************************************************************************************/
#include <cmsis_dap.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************/
/* Global Constants, Macros and Type Definitions                                                  */
/**************************************************************************************************/

/* clang-format off */
/**
 * @brief Set IO to input or output
 * @param uint8_t gpio to set
 * @param uint8_t direction 1 = output, 0 = input
 * @param uint8_t option 0 = no-pull, 1 = pull-up, 2 = pull-down, 3 = disconnect
 */
#define ID_DAP_VENDOR_SET_IO_DIR        ID_DAP_VENDOR31
/**
 * @brief Set IO to high or low
 * @param uint8_t gpio to set
 * @param uint8_t level 1 = high, 0 = low
 */
#define ID_DAP_VENDOR_SET_IO            (ID_DAP_VENDOR31 - 1)
/**
 * @brief Read IO state
 * @param uint8_t gpio to read
 * @return uint8_t < 0 error, 1 = high, 0 = low
 */
#define ID_DAP_VENDOR_READ_IO           	(ID_DAP_VENDOR31 - 2)

/**
 * @brief Deprecated EEPROM read/write functions. Use read/write settings instead.
 * These defines are here for informational purposes only.
 */
#define ID_DAP_VENDOR_READ_BOARD_ID_BYTES   (ID_DAP_VENDOR31 - 3)
#define ID_DAP_VENDOR_WRITE_BOARD_ID_BYTES   (ID_DAP_VENDOR31 - 4)

/**
 * @brief Reboot the debug probe
 * @param uint8_t reboot to bootloader if 1, else reboot and run application
 */
#define ID_DAP_VENDOR_REBOOT                 (ID_DAP_VENDOR31 - 5)

/**
 * @brief Read settings from probe (internal flash)
 * @return bytes read. length of bytes read is always 256 bytes. < 0 indicates error
 */
#define ID_DAP_VENDOR_READ_SETTINGS          (ID_DAP_VENDOR31 - 6)

/**
 * @brief Write settings to probe (internal flash)
 * @param uint8_t number of bytes to write (max 256)
 * @param uint8_t* bytes to write. This will always be 256 bytes (a full settings page)
 * @return int8_t result 0 on success, < 0 indicates error
 */
#define ID_DAP_VENDOR_WRITE_SETTINGS         (ID_DAP_VENDOR31 - 7)

/* clang-format off */

enum {
	DAP_VENDOR_ERR_INVALID_IO = 1,
	DAP_VENDOR_ERR_INVALID_IO_OPTION,
	DAP_VENDOR_ERR_INVALID_SIZE,
};

enum {
	IO_OPTION_NO_PULL = 0,
	IO_OPTION_PULL_UP,
	IO_OPTION_PULL_DOWN,
	IO_OPTION_DISCONNECT,
	IO_OPTION_INVALID,
};

/**************************************************************************************************/
/* Global Function Prototypes                                                                     */
/**************************************************************************************************/
uint16_t dap_vendor_cmd_handler(uint8_t cmd_id,
				      const uint8_t *request,
				      uint8_t *response);

#ifdef __cplusplus
}
#endif

#endif /* __DAP_VENDOR_H__ */
