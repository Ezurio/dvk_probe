/*
 * Copyright 2025 Google LLC
 * Copyright 2025 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

#ifndef RFPROS_UART_BRIDGE_H
#define RFPROS_UART_BRIDGE_H

#include <zephyr/device.h>

/**
 * @brief Update the hardware port settings on a uart bridge
 *
 * If dev is part bridge_dev, then the dev uart configuration are applied to
 * the other device in the uart bridge. This allows propagating the settings
 * from a USB CDC-ACM port to a hardware UART.
 *
 * If dev is not part of bridge_dev then the function is a no-op.
 */
void uart_bridge_settings_update(const struct device *dev, const struct device *bridge_dev);

/**
 * @brief Get the peer device in a uart bridge
 *
 * @param dev The device to find the peer for
 * @param bridge_dev The uart bridge device
 * @return const struct device* The peer device, or NULL if not found
 */
const struct device *uart_bridge_get_peer(const struct device *dev,
					  const struct device *bridge_dev);

#endif /* RFPROS_UART_BRIDGE_H */
