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
 * Microsoft OS Descriptors for Windows compatibility.
 * - MSOS v1 for Windows 7 and earlier
 * - MSOS v2 for Windows 8.1 and later
 * See Microsoft OS Descriptors Specification for reference.
 */

#define APP_MSOS_VENDOR_CODE 0x14U

/* MSOS v1 definitions (Windows 7) */
#define MSOS1_STRING_DESCRIPTOR_INDEX 0xEE

/* MSOS v2 definitions (Windows 8.1+) */
/* Windows version (8.1) */
#define APP_MSOS2_OS_VERSION 0x06030000UL

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

/*
 * Microsoft OS 1.0 Extended Compat ID OS Feature Descriptor
 * For Windows 7 compatibility
 */
struct msos1_compat_id_function {
	uint8_t bFirstInterfaceNumber;
	uint8_t reserved1;
	uint8_t compatibleID[8];
	uint8_t subCompatibleID[8];
	uint8_t reserved2[6];
} __packed;

struct msos1_compat_id_descriptor {
	uint32_t dwLength;
	uint16_t bcdVersion;
	uint16_t wIndex;
	uint8_t bCount;
	uint8_t reserved[7];
	struct msos1_compat_id_function function[CDC_ACM_INSTANCE_COUNT + 1];
} __packed;

const struct msos1_compat_id_descriptor msos1_compat_id_desc = {
	.dwLength = sys_cpu_to_le32(sizeof(struct msos1_compat_id_descriptor)),
	.bcdVersion = sys_cpu_to_le16(0x0100),
	.wIndex = sys_cpu_to_le16(0x0004),    /* Extended compat ID descriptor */
	.bCount = CDC_ACM_INSTANCE_COUNT + 1, /* CDC ACM interfaces + DAP interface */
	.reserved = {0},
	.function =
		{
#if CDC_ACM_INSTANCE_COUNT > 0
			{
				.bFirstInterfaceNumber = 0, /* First CDC ACM control interface */
				.reserved1 = 1,
				.compatibleID = {0}, /* Use default CDC driver */
				.subCompatibleID = {0},
				.reserved2 = {0},
			},
#endif
#if CDC_ACM_INSTANCE_COUNT > 1
			{
				.bFirstInterfaceNumber = 2, /* Second CDC ACM control interface */
				.reserved1 = 1,
				.compatibleID = {0},
				.subCompatibleID = {0},
				.reserved2 = {0},
			},
#endif
#if CDC_ACM_INSTANCE_COUNT > 2
			{
				.bFirstInterfaceNumber = 4, /* Third CDC ACM control interface */
				.reserved1 = 1,
				.compatibleID = {0},
				.subCompatibleID = {0},
				.reserved2 = {0},
			},
#endif
			{
				.bFirstInterfaceNumber = DAP_INTERFACE_NUMBER,
				.reserved1 = 1,
				.compatibleID = {'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00},
				.subCompatibleID = {0},
				.reserved2 = {0},
			},
		},
};

/* MSOS v1 OS String Descriptor for signaling MSOS support to Windows 7 */
struct msos1_os_string_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t qwSignature[7]; /* "MSFT100" in UTF-16LE */
	uint8_t bMS_VendorCode;
	uint8_t bPad;
} __packed;

const struct msos1_os_string_descriptor msos1_os_string_desc = {
	.bLength = sizeof(struct msos1_os_string_descriptor),
	.bDescriptorType = USB_DESC_STRING,
	.qwSignature =
		{
			sys_cpu_to_le16('M'),
			sys_cpu_to_le16('S'),
			sys_cpu_to_le16('F'),
			sys_cpu_to_le16('T'),
			sys_cpu_to_le16('1'),
			sys_cpu_to_le16('0'),
			sys_cpu_to_le16('0'),
		},
	.bMS_VendorCode = APP_MSOS_VENDOR_CODE,
	.bPad = 0,
};

/*
 * Microsoft OS 2.0 Descriptors
 * For Windows 8.1+ compatibility
 */
struct msosv2_descriptor {
	struct msosv2_descriptor_set_header header;
	/* DAP interface function subset header */
	struct msosv2_function_subset_header dap_subset_header;
	struct msosv2_compatible_id compatible_id;
	struct msosv2_guids_property guids_property;
} __packed;

const struct msosv2_descriptor msosv2_desc = {
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

const struct bos_msosv2_descriptor bos_msosv2_desc = {
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
		.bMS_VendorCode = APP_MSOS_VENDOR_CODE,
		.bAltEnumCode = 0x00},
};

static int msos_to_host_cb(const struct usbd_context *const ctx,
			   const struct usb_setup_packet *const setup, struct net_buf *const buf)
{
	LOG_INF("Vendor callback to host");

	/* Handle MSOS v1 requests (Windows 7) */
	if (setup->bRequest == APP_MSOS_VENDOR_CODE && setup->wIndex == 0x0004) {
		/* Extended Compat ID OS Feature Descriptor */
		LOG_INF("Get MS OS 1.0 Extended Compat ID Descriptor");
		net_buf_add_mem(buf, &msos1_compat_id_desc,
				MIN(net_buf_tailroom(buf), sizeof(msos1_compat_id_desc)));
		return 0;
	} else if (setup->bRequest == APP_MSOS_VENDOR_CODE &&
		   setup->wIndex == MS_OS_20_DESCRIPTOR_INDEX) {
		/* Handle MSOS v2 requests (Windows 8.1+) */
		LOG_INF("Get MS OS 2.0 Descriptor Set");
		net_buf_add_mem(buf, &msosv2_desc, MIN(net_buf_tailroom(buf), sizeof(msosv2_desc)));
		return 0;
	}

	return -ENOTSUP;
}

/* Register MSOS v2 BOS descriptor with vendor request callback (handles both v1 and v2) */
USBD_DESC_BOS_VREQ_DEFINE(bos_vreq_msosv2, sizeof(bos_msosv2_desc), &bos_msosv2_desc,
			  APP_MSOS_VENDOR_CODE, msos_to_host_cb, NULL);

/* MSOS v1 OS String Descriptor node for Windows 7 support */
struct usbd_desc_node msos1_os_string_node = {
	.str =
		{
			.idx = MSOS1_STRING_DESCRIPTOR_INDEX,
			/* Reuse interface type for special string */
			.utype = USBD_DUT_STRING_INTERFACE,
		},
	.ptr = &msos1_os_string_desc,
	.bLength = sizeof(msos1_os_string_desc),
	.bDescriptorType = USB_DESC_STRING,
};

#endif /* APP_MSOSV2_DESCRIPTOR_H */
