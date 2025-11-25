/*
 * Copyright (c) 2023 Nordic Semiconductor ASA.
 * Copyright 2025 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

#ifndef APP_USBD_H
#define APP_USBD_H

#include <stdint.h>
#include <zephyr/usb/usbd.h>

/*
 * This function configures and initializes a
 * USB device. It configures the device context, default string descriptors,
 * USB device configuration, registers any available class instances, and
 * finally initializes USB device.
 *
 * It returns the configured and initialized USB device context on success,
 * otherwise it returns NULL.
 */
struct usbd_context *app_usbd_init_device(usbd_msg_cb_t msg_cb);

/*
 * This function is similar to app_usbd_init_device(), but does not
 * initialize the device. It allows the application to set additional features,
 * such as additional descriptors.
 */
struct usbd_context *app_usbd_setup_device(usbd_msg_cb_t msg_cb);

#endif /* APP_USBD_H */
