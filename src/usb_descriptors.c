/*
 * usb_descriptors.c
 * =================
 * All USB descriptors for the RP2350A composite HID device.
 *
 * VID/PID = Logitech G502 (046D:C08B) so Windows treats it as a normal mouse.
 *
 * KEY POINT: bInterval = 1 on the HID IN endpoint = 1 ms = 1000 Hz.
 *
 * Three HID top-level collections on one interface:
 *   Report ID 1  IN  4 B   Mouse       (buttons + X + Y + wheel)
 *   Report ID 3  IN  8 B   Boot KB     (modifier + reserved + 6 keycodes)
 *   Report ID 2  OUT 7 B   Vendor cmd  (aim / key-inject / update)
 */

#include "tusb.h"
#include <string.h>

/* ── Device Descriptor ──────────────────────────────────────────────────── */
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x046D,
    .idProduct          = 0xC08B,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x00,
    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

/* ── HID Report Descriptor ──────────────────────────────────────────────── */
static uint8_t const hid_report_descriptor[] =
{
    /* ── Mouse, Report ID 1, IN 4 bytes ── */
    0x05, 0x01,        /* Usage Page: Generic Desktop */
    0x09, 0x02,        /* Usage: Mouse */
    0xA1, 0x01,        /* Collection: Application */
      0x85, 0x01,      /*   Report ID 1 */
      0x09, 0x01,      /*   Usage: Pointer */
      0xA1, 0x00,      /*   Collection: Physical */
        /* Buttons 1-5 */
        0x05, 0x09, 0x19, 0x01, 0x29, 0x05,
        0x15, 0x00, 0x25, 0x01,
        0x95, 0x05, 0x75, 0x01, 0x81, 0x02,
        /* Pad 3 bits */
        0x95, 0x01, 0x75, 0x03, 0x81, 0x01,
        /* X, Y -127..127 */
        0x05, 0x01, 0x09, 0x30, 0x09, 0x31,
        0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06,
        /* Wheel */
        0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x81, 0x06,
      0xC0,
    0xC0,

    /* ── Boot Keyboard, Report ID 3, IN 8 bytes ── */
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
      0x85, 0x03,
      0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
      0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
      0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
      0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x26, 0xFF, 0x00,
      0x05, 0x07, 0x19, 0x00, 0x29, 0xFF, 0x81, 0x00,
    0xC0,

    /* ── Vendor cmd channel, Report ID 2, OUT 7 bytes ── */
    0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01,
      0x85, 0x02,
      0x09, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00,
      0x75, 0x08, 0x95, 0x07, 0x91, 0x02,
    0xC0,
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
    (void)itf;
    return hid_report_descriptor;
}

/* ── Configuration Descriptor ───────────────────────────────────────────── */
/*
 * We build the config descriptor by hand so we can set bInterval=1 exactly.
 *
 * Layout (total = 9 + 9 + 9 + 7 + 7 = 41 bytes):
 *   [0]  Configuration descriptor  (9 bytes)
 *   [9]  Interface descriptor      (9 bytes)
 *  [18]  HID descriptor            (9 bytes)
 *  [27]  Endpoint IN  descriptor   (7 bytes)  bInterval=1
 *  [34]  Endpoint OUT descriptor   (7 bytes)  bInterval=1
 */
#define EP_HID_IN   0x81
#define EP_HID_OUT  0x01
#define EP_SIZE     16
#define REPORT_DESC_LEN  ((uint16_t)sizeof(hid_report_descriptor))
#define CONFIG_TOTAL_LEN (41 + TUD_CDC_DESC_LEN)

static uint8_t const config_descriptor[CONFIG_TOTAL_LEN] =
{
    /* Configuration descriptor */
    9,                          /* bLength */
    TUSB_DESC_CONFIGURATION,    /* bDescriptorType */
    CONFIG_TOTAL_LEN & 0xFF, CONFIG_TOTAL_LEN >> 8,     /* wTotalLength LE */
    3,                          /* bNumInterfaces */
    1,                          /* bConfigurationValue */
    0,                          /* iConfiguration */
    0x80,                       /* bmAttributes: bus-powered */
    50,                         /* bMaxPower: 100 mA */

    /* Interface descriptor */
    9,                          /* bLength */
    TUSB_DESC_INTERFACE,        /* bDescriptorType */
    0,                          /* bInterfaceNumber */
    0,                          /* bAlternateSetting */
    2,                          /* bNumEndpoints (IN + OUT) */
    TUSB_CLASS_HID,             /* bInterfaceClass */
    0,                          /* bInterfaceSubClass (0 = no boot) */
    HID_ITF_PROTOCOL_NONE,      /* bInterfaceProtocol */
    0,                          /* iInterface */

    /* HID descriptor */
    9,                          /* bLength */
    HID_DESC_TYPE_HID,          /* bDescriptorType */
    0x11, 0x01,                 /* bcdHID 1.11 */
    0,                          /* bCountryCode */
    1,                          /* bNumDescriptors */
    HID_DESC_TYPE_REPORT,       /* bDescriptorType[0] */
    (uint8_t)(REPORT_DESC_LEN & 0xFF),
    (uint8_t)(REPORT_DESC_LEN >> 8),

    /* Endpoint IN (device -> PC) bInterval = 1 = 1ms = 1000 Hz */
    7,                          /* bLength */
    TUSB_DESC_ENDPOINT,         /* bDescriptorType */
    EP_HID_IN,                  /* bEndpointAddress */
    TUSB_XFER_INTERRUPT,        /* bmAttributes */
    EP_SIZE, 0x00,              /* wMaxPacketSize LE */
    1,                          /* bInterval = 1ms  <-- 1000 Hz */

    /* Endpoint OUT (PC -> device) bInterval = 1 */
    7,                          /* bLength */
    TUSB_DESC_ENDPOINT,         /* bDescriptorType */
    EP_HID_OUT,                 /* bEndpointAddress */
    TUSB_XFER_INTERRUPT,        /* bmAttributes */
    EP_SIZE, 0x00,              /* wMaxPacketSize LE */
    1,                          /* bInterval = 1ms */

    /* CDC serial (debug) */
    TUD_CDC_DESCRIPTOR(1, 0, 0x82, 8, 0x03, 0x83, 64)
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return config_descriptor;
}

/* ── String Descriptors ─────────────────────────────────────────────────── */
static char const *string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 },  /* 0: English */
    "Logitech",                     /* 1: Manufacturer */
    "Gaming Mouse G502",            /* 2: Product */
};

static uint16_t _desc_str[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= (uint8_t)(sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
            return NULL;
        const char *str = string_desc_arr[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++)
            _desc_str[1 + i] = str[i];
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
