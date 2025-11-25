/*
 * Copyright (c) 2016-2019 Intel Corporation
 * Copyright (c) 2023-2024 Nordic Semiconductor ASA
 * Copyright 2025 Ezurio
 *
 * SPDX-License-Identifier: LicenseRef-Ezurio-Clause
 */

#ifndef APP_MSOSV2_DESCRIPTOR_H
#define APP_MSOSV2_DESCRIPTOR_H

/*
 * Microsoft OS 2.0 platform capability and Microsoft OS 2.0 descriptor set.
 * See Microsoft OS 2.0 Descriptors Specification for reference.
 */

#define APP_MSOS2_VENDOR_CODE 0x14U
/* Windows version (8.1) */
#define APP_MSOS2_OS_VERSION  0x06030000UL

/* {F9E8D7C6-B5A4-3210-DCBA-0987654321FE} */
#define CMSIS_DAP_V2_DEVICE_INTERFACE_GUID                                                         \
	'{', 0x00, 'F', 0x00, '9', 0x00, 'E', 0x00, '8', 0x00, 'D', 0x00, '7', 0x00, 'C', 0x00,    \
		'6', 0x00, '-', 0x00, 'B', 0x00, '5', 0x00, 'A', 0x00, '4', 0x00, '-', 0x00, '3',  \
		0x00, '2', 0x00, '1', 0x00, '0', 0x00, '-', 0x00, 'D', 0x00, 'C', 0x00, 'B', 0x00, \
		'A', 0x00, '-', 0x00, '0', 0x00, '9', 0x00, '8', 0x00, '7', 0x00, '6', 0x00, '5',  \
		0x00, '4', 0x00, '3', 0x00, '2', 0x00, '1', 0x00, 'F', 0x00, 'E', 0x00, '}', 0x00, \
		0x00, 0x00, 0x00, 0x00

/*
 * Calculate the DAP interface number dynamically based on CDC ACM instances.
 * Each CDC ACM instance uses 2 interfaces (control + data), so the DAP interface
 * comes after all CDC ACM interfaces.
 */
#define CDC_ACM_INSTANCE_COUNT DT_NUM_INST_STATUS_OKAY(zephyr_cdc_acm_uart)
#define DAP_INTERFACE_NUMBER   (CDC_ACM_INSTANCE_COUNT * 2)

/*
 * The DAP function subset contains the WinUSB compatible ID and device interface GUID.
 */
#define DAP_FUNCTION_SUBSET_LENGTH                                                                 \
	(sizeof(struct msosv2_function_subset_header) + sizeof(struct msosv2_compatible_id) +      \
	 sizeof(struct msosv2_guids_property))

struct msosv2_descriptor {
	struct msosv2_descriptor_set_header header;
	/* DAP interface function subset header */
	struct msosv2_function_subset_header dap_subset_header;
	struct msosv2_compatible_id compatible_id;
	struct msosv2_guids_property guids_property;
} __packed;

static struct msosv2_descriptor msosv2_desc = {
	.header =
		{
			.wLength = sizeof(struct msosv2_descriptor_set_header),
			.wDescriptorType = MS_OS_20_SET_HEADER_DESCRIPTOR,
			.dwWindowsVersion = sys_cpu_to_le32(APP_MSOS2_OS_VERSION),
			.wTotalLength = sizeof(msosv2_desc),
		},
	/* DAP interface number is calculated dynamically based on CDC ACM instances */
	.dap_subset_header =
		{
			.wLength = sizeof(struct msosv2_function_subset_header),
			.wDescriptorType = MS_OS_20_SUBSET_HEADER_FUNCTION,
			.bFirstInterface = DAP_INTERFACE_NUMBER,
			.bReserved = 0,
			.wSubsetLength = DAP_FUNCTION_SUBSET_LENGTH,
		},
	.compatible_id =
		{
			.wLength = sizeof(struct msosv2_compatible_id),
			.wDescriptorType = MS_OS_20_FEATURE_COMPATIBLE_ID,
			.CompatibleID = {'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00},
		},
	.guids_property =
		{
			.wLength = sizeof(struct msosv2_guids_property),
			.wDescriptorType = MS_OS_20_FEATURE_REG_PROPERTY,
			.wPropertyDataType = MS_OS_20_PROPERTY_DATA_REG_MULTI_SZ,
			.wPropertyNameLength = 42,
			.PropertyName = {DEVICE_INTERFACE_GUIDS_PROPERTY_NAME},
			.wPropertyDataLength = 80,
			.bPropertyData = {CMSIS_DAP_V2_DEVICE_INTERFACE_GUID},
		},
};

struct bos_msosv2_descriptor {
	struct usb_bos_platform_descriptor platform;
	struct usb_bos_capability_msos cap;
} __packed;

struct bos_msosv2_descriptor bos_msosv2_desc = {
	/*
	 * Microsoft OS 2.0 Platform Capability Descriptor,
	 * see Microsoft OS 2.0 Descriptors Specification
	 */
	.platform =
		{
			.bLength = sizeof(struct usb_bos_platform_descriptor) +
				   sizeof(struct usb_bos_capability_msos),
			.bDescriptorType = USB_DESC_DEVICE_CAPABILITY,
			.bDevCapabilityType = USB_BOS_CAPABILITY_PLATFORM,
			.bReserved = 0,
			/* Microsoft OS 2.0 descriptor platform capability UUID
			 * D8DD60DF-4589-4CC7-9CD2-659D9E648A9F
			 */
			.PlatformCapabilityUUID =
				{
					0xDF,
					0x60,
					0xDD,
					0xD8,
					0x89,
					0x45,
					0xC7,
					0x4C,
					0x9C,
					0xD2,
					0x65,
					0x9D,
					0x9E,
					0x64,
					0x8A,
					0x9F,
				},
		},
	.cap = {.dwWindowsVersion = sys_cpu_to_le32(APP_MSOS2_OS_VERSION),
		.wMSOSDescriptorSetTotalLength = sys_cpu_to_le16(sizeof(msosv2_desc)),
		.bMS_VendorCode = APP_MSOS2_VENDOR_CODE,
		.bAltEnumCode = 0x00},
};

static int msosv2_to_host_cb(const struct usbd_context *const ctx,
			     const struct usb_setup_packet *const setup, struct net_buf *const buf)
{
	LOG_INF("Vendor callback to host");

	if (setup->bRequest == APP_MSOS2_VENDOR_CODE &&
	    setup->wIndex == MS_OS_20_DESCRIPTOR_INDEX) {
		LOG_INF("Get MS OS 2.0 Descriptor Set");

		net_buf_add_mem(buf, &msosv2_desc, MIN(net_buf_tailroom(buf), sizeof(msosv2_desc)));

		return 0;
	}

	return -ENOTSUP;
}

USBD_DESC_BOS_VREQ_DEFINE(bos_vreq_msosv2, sizeof(bos_msosv2_desc), &bos_msosv2_desc,
			  APP_MSOS2_VENDOR_CODE, msosv2_to_host_cb, NULL);

#endif /* APP_MSOSV2_DESCRIPTOR_H */
