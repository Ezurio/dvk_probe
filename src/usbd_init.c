/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 * Copyright 2025 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>
#include <zephyr/usb/msos_desc.h>
#include <cmsis_dap.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dvk_probe_usbd_config, CONFIG_DVK_PROBE_LOG_LEVEL);

#include "msosv2.h"

/* By default, do not register the USB DFU class DFU mode instance. */
static const char *const blocklist[] = {
	"dfu_dfu",
	NULL,
};

/* doc device instantiation start */
/*
 * Instantiate a context named app_usbd using the default USB device
 * controller, the vendor ID, and the product ID.
 */
USBD_DEVICE_DEFINE(app_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_APP_USBD_VID, CONFIG_APP_USBD_PID);
/* doc device instantiation end */

/* doc string instantiation start */
USBD_DESC_LANG_DEFINE(app_lang);
USBD_DESC_MANUFACTURER_DEFINE(app_mfr, CONFIG_APP_USBD_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(app_product, CONFIG_APP_USBD_PRODUCT);
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(app_sn)));

/* doc string instantiation end */

USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "HS Configuration");

/* doc configuration instantiation start */
static const uint8_t attributes = (IS_ENABLED(CONFIG_APP_USBD_SELF_POWERED) ?
				   USB_SCD_SELF_POWERED : 0) |
				  (IS_ENABLED(CONFIG_APP_USBD_REMOTE_WAKEUP) ?
				   USB_SCD_REMOTE_WAKEUP : 0);

/* Full speed configuration */
USBD_CONFIGURATION_DEFINE(app_fs_config,
			  attributes,
			  CONFIG_APP_USBD_MAX_POWER, &fs_cfg_desc);

/* High speed configuration */
USBD_CONFIGURATION_DEFINE(app_hs_config,
			  attributes,
			  CONFIG_APP_USBD_MAX_POWER, &hs_cfg_desc);
/* doc configuration instantiation end */

#if CONFIG_APP_USBD_20_EXTENSION_DESC
/*
 * This does not yet provide valuable information, but rather serves as an
 * example, and will be improved in the future.
 */
static const struct usb_bos_capability_lpm bos_cap_lpm = {
	.bLength = sizeof(struct usb_bos_capability_lpm),
	.bDescriptorType = USB_DESC_DEVICE_CAPABILITY,
	.bDevCapabilityType = USB_BOS_CAPABILITY_EXTENSION,
	.bmAttributes = 0UL,
};

USBD_DESC_BOS_DEFINE(app_usbext, sizeof(bos_cap_lpm), &bos_cap_lpm);
#endif

static void app_fix_code_triple(struct usbd_context *uds_ctx,
				   const enum usbd_speed speed)
{
	/* Always use class code information from Interface Descriptors */
	if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_CDC_NCM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_MIDI2_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_AUDIO2_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_VIDEO_CLASS)) {
		/*
		 * Class with multiple interfaces have an Interface
		 * Association Descriptor available, use an appropriate triple
		 * to indicate it.
		 */
		usbd_device_set_code_triple(uds_ctx, speed,
					    USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	} else {
		usbd_device_set_code_triple(uds_ctx, speed, 0, 0, 0);
	}
}

struct usbd_context *app_usbd_setup_device(usbd_msg_cb_t msg_cb)
{
	int err;

	/* doc add string descriptor start */
	err = usbd_add_descriptor(&app_usbd, &app_lang);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&app_usbd, &app_mfr);
	if (err) {
		LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&app_usbd, &app_product);
	if (err) {
		LOG_ERR("Failed to initialize product descriptor (%d)", err);
		return NULL;
	}

	IF_ENABLED(CONFIG_HWINFO, (
		err = usbd_add_descriptor(&app_usbd, &app_sn);
	))
	if (err) {
		LOG_ERR("Failed to initialize SN descriptor (%d)", err);
		return NULL;
	}
	/* doc add string descriptor end */

	if (USBD_SUPPORTS_HIGH_SPEED &&
	    usbd_caps_speed(&app_usbd) == USBD_SPEED_HS) {
		err = usbd_add_configuration(&app_usbd, USBD_SPEED_HS,
					     &app_hs_config);
		if (err) {
			LOG_ERR("Failed to add High-Speed configuration");
			return NULL;
		}

		err = usbd_register_all_classes(&app_usbd, USBD_SPEED_HS, 1,
						blocklist);
		if (err) {
			LOG_ERR("Failed to add register classes");
			return NULL;
		}

		app_fix_code_triple(&app_usbd, USBD_SPEED_HS);
	}

	/* doc configuration register start */
	err = usbd_add_configuration(&app_usbd, USBD_SPEED_FS,
				     &app_fs_config);
	if (err) {
		LOG_ERR("Failed to add Full-Speed configuration");
		return NULL;
	}
	/* doc configuration register end */

	/* doc functions register start */
	err = usbd_register_all_classes(&app_usbd, USBD_SPEED_FS, 1, blocklist);
	if (err) {
		LOG_ERR("Failed to add register classes");
		return NULL;
	}
	/* doc functions register end */

	app_fix_code_triple(&app_usbd, USBD_SPEED_FS);
	usbd_self_powered(&app_usbd, attributes & USB_SCD_SELF_POWERED);

	if (msg_cb != NULL) {
		/* doc device init-and-msg start */
		err = usbd_msg_register_cb(&app_usbd, msg_cb);
		if (err) {
			LOG_ERR("Failed to register message callback");
			return NULL;
		}
		/* doc device init-and-msg end */
	}

	/* Set USB version to 2.01 to enable BOS descriptor support */
	(void)usbd_device_set_bcd_usb(&app_usbd, USBD_SPEED_FS, 0x0201);
	if (USBD_SUPPORTS_HIGH_SPEED &&
	    usbd_caps_speed(&app_usbd) == USBD_SPEED_HS) {
		(void)usbd_device_set_bcd_usb(&app_usbd, USBD_SPEED_HS, 0x0201);
	}

#if CONFIG_APP_USBD_20_EXTENSION_DESC
	err = usbd_add_descriptor(&app_usbd, &app_usbext);
	if (err) {
		LOG_ERR("Failed to add USB 2.0 Extension Descriptor");
		return NULL;
	}
#endif

	err = usbd_add_descriptor(&app_usbd, &bos_vreq_msosv2);
	if (err) {
		LOG_ERR("Failed to add MSOSv2 capability descriptor");
		return NULL;
	}

	return &app_usbd;
}

struct usbd_context *app_usbd_init_device(usbd_msg_cb_t msg_cb)
{
	int err;

	if (app_usbd_setup_device(msg_cb) == NULL) {
		return NULL;
	}

	/* doc device init start */
	err = usbd_init(&app_usbd);
	if (err) {
		LOG_ERR("Failed to initialize device support");
		return NULL;
	}
	/* doc device init end */

	return &app_usbd;
}
