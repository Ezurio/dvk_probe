/**
 * @file probe_settings.h
 * @brief API for probe NV settings
 *
 * Copyright (c) 2026 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

#ifndef __PROBE_SETTINGS_H__
#define __PROBE_SETTINGS_H__

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************/
/* Global Constants, Macros and Type Definitions                                                  */
/**************************************************************************************************/
/* clang-format off */
#define PROBE_SETTINGS_MAX_SIZE 	256
#define PROBE_SETTINGS_INVALID_FF   0xFF
#define PROBE_SETTINGS_INVALID_00   0x00
#define PROBE_SETTINGS_V1       	0x01
#define PROBE_SETTINGS_V2      		0x02
/* clang-format on */

#pragma pack(1)
typedef struct {
	uint8_t version;
	uint8_t data[PROBE_SETTINGS_MAX_SIZE - 1];
} probe_settings_base_t;

typedef struct {
	uint8_t version;
	char target_device_vendor[32];
	char target_device_name[32];
	char target_board_vendor[32];
	char target_board_name[32];
} probe_settings_v1_t;

typedef struct {
	uint8_t version;
	char target_device_vendor[32];
	char target_device_name[32];
	char target_board_vendor[32];
	char target_board_name[32];
	uint16_t usb_vid;
	uint16_t usb_pid;
} probe_settings_v2_t;

typedef union {
	probe_settings_base_t base;
	probe_settings_v1_t v1;
	probe_settings_v2_t v2;
} probe_settings_ut;
#pragma pack()

enum {
	SETTINGS_ERR_INVALID_PARAM = 1,
};

/**************************************************************************************************/
/* Global Data Definitions                                                                        */
/**************************************************************************************************/
extern const probe_settings_ut *probe_settings;

/**************************************************************************************************/
/* Global Function Prototypes                                                                     */
/**************************************************************************************************/
void probe_settings_init(void);

/**
 * @brief Write settings to internal flash
 *
 * @param settings pointer to settings data
 * @param size size of settings data
 * @return int 0 on success, < 0 on failure
 */
int write_internal_settings(const probe_settings_ut *settings, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* __PROBE_SETTINGS_H__ */
