/*
 * usb_descriptors.c
 * =================
 * TinyUSB device descriptor callbacks.
 *
 * PID/VID deliberately set to match a Logitech G502 (046D:C08B) so the PC
 * sees a normal mouse. Change to any value you like.
 *
 * Key detail: the HID interrupt IN endpoint uses bInterval = 1 (1 ms),
 * giving the full-speed USB maximum of 1000 reports per second.
 *
 * HID Report descriptor encodes three top-level collections:
 *   Collection 1 - Mouse        (Usage Page 0x01, Usage 0x02)
 *                  Report ID 1  IN  4 bytes
 *   Collection 2 - Keyboard     (Usage Page 0x01, Usage 0x06)
 *                  Report ID 3  IN  8 bytes
 *   Collection 3 - Vendor       (Usage Page 0xFF00, Usage 0x01)
 *                  Report ID 2  OUT 7 bytes (command channel)
 */

#include "tusb.h"

/* ──────────────────────────────────────────────────────────────────────────── */
/*  Device descriptor                                                           */
/* ──────────────────────────────────────────────────────────────────────────── */
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x046D,   /* Logitech */
    .idProduct          = 0xC08B,   /* G502 wired */
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x00,
    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

/* ──────────────────────────────────────────────────────────────────────────── */
/*  HID Report Descriptor                                                       */
/* ──────────────────────────────────────────────────────────────────────────── */
static uint8_t const hid_report_descriptor[] = {
    /* ── Collection 1: Mouse (Report ID 1, IN 4 bytes) ── */
    0x05, 0x01,        /* Usage Page: Generic Desktop */
    0x09, 0x02,        /* Usage: Mouse */
    0xA1, 0x01,        /* Collection: Application */
      0x09, 0x01,      /*   Usage: Pointer */
      0xA1, 0x00,      /*   Collection: Physical */
        0x85, 0x01,    /*     Report ID 1 */
        /* Buttons 1-5 */
        0x05, 0x09,    /*     Usage Page: Buttons */
        0x19, 0x01,    /*     Usage Min: 1 */
        0x29, 0x05,    /*     Usage Max: 5 */
        0x15, 0x00,    /*     Logical Min: 0 */
        0x25, 0x01,    /*     Logical Max: 1 */
        0x95, 0x05,    /*     Report Count: 5 */
        0x75, 0x01,    /*     Report Size: 1 */
        0x81, 0x02,    /*     Input: Data, Variable, Absolute */
        /* Padding: 3 bits */
        0x95, 0x01,    /*     Report Count: 1 */
        0x75, 0x03,    /*     Report Size: 3 */
        0x81, 0x01,    /*     Input: Constant */
        /* X, Y: -127..127 */
        0x05, 0x01,    /*     Usage Page: Generic Desktop */
        0x09, 0x30,    /*     Usage: X */
        0x09, 0x31,    /*     Usage: Y */
        0x15, 0x81,    /*     Logical Min: -127 */
        0x25, 0x7F,    /*     Logical Max: 127 */
        0x75, 0x08,    /*     Report Size: 8 */
        0x95, 0x02,    /*     Report Count: 2 */
        0x81, 0x06,    /*     Input: Data, Variable, Relative */
        /* Wheel */
        0x09, 0x38,    /*     Usage: Wheel */
        0x15, 0x81,    /*     Logical Min: -127 */
        0x25, 0x7F,    /*     Logical Max: 127 */
        0x75, 0x08,    /*     Report Size: 8 */
        0x95, 0x01,    /*     Report Count: 1 */
        0x81, 0x06,    /*     Input: Data, Variable, Relative */
      0xC0,            /*   End Collection */
    0xC0,              /* End Collection */

    /* ── Collection 2: Boot Keyboard (Report ID 3, IN 8 bytes) ── */
    0x05, 0x01,        /* Usage Page: Generic Desktop */
    0x09, 0x06,        /* Usage: Keyboard */
    0xA1, 0x01,        /* Collection: Application */
      0x85, 0x03,      /*   Report ID 3 */
      /* Modifier keys */
      0x05, 0x07,      /*   Usage Page: Keyboard */
      0x19, 0xE0,      /*   Usage Min: LCTRL */
      0x29, 0xE7,      /*   Usage Max: RGUI  */
      0x15, 0x00,      /*   Logical Min: 0 */
      0x25, 0x01,      /*   Logical Max: 1 */
      0x75, 0x01,      /*   Report Size: 1 */
      0x95, 0x08,      /*   Report Count: 8 */
      0x81, 0x02,      /*   Input: Data, Variable, Absolute */
      /* Reserved byte */
      0x95, 0x01,      /*   Report Count: 1 */
      0x75, 0x08,      /*   Report Size: 8 */
      0x81, 0x01,      /*   Input: Constant */
      /* 6 keycodes */
      0x95, 0x06,      /*   Report Count: 6 */
      0x75, 0x08,      /*   Report Size: 8 */
      0x15, 0x00,      /*   Logical Min: 0 */
      0x26, 0xFF, 0x00,/*   Logical Max: 255 */
      0x05, 0x07,      /*   Usage Page: Keyboard */
      0x19, 0x00,      /*   Usage Min: 0 */
      0x29, 0xFF,      /*   Usage Max: 255 */
      0x81, 0x00,      /*   Input: Data, Array */
    0xC0,              /* End Collection */

    /* ── Collection 3: Vendor command channel (Report ID 2, OUT 7 bytes) ── */
    0x06, 0x00, 0xFF,  /* Usage Page: Vendor Defined (0xFF00) */
    0x09, 0x01,        /* Usage: Vendor Usage 1 */
    0xA1, 0x01,        /* Collection: Application */
      0x85, 0x02,      /*   Report ID 2 */
      0x09, 0x02,      /*   Usage: Vendor Usage 2 */
      0x15, 0x00,      /*   Logical Min: 0 */
      0x26, 0xFF, 0x00,/*   Logical Max: 255 */
      0x75, 0x08,      /*   Report Size: 8 */
      0x95, 0x07,      /*   Report Count: 7 */
      0x91, 0x02,      /*   Output: Data, Variable, Absolute */
    0xC0,              /* End Collection */
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
    (void)itf;
    return hid_report_descriptor;
}

/* ──────────────────────────────────────────────────────────────────────────── */
/*  Configuration descriptor                                                    */
/*  One interface: HID, one IN endpoint (bInterval=1) + one OUT endpoint       */
/* ──────────────────────────────────────────────────────────────────────────── */

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN)
#define EPNUM_HID_IN   0x81
#define EPNUM_HID_OUT  0x01

/* TUD_HID_INOUT_DESCRIPTOR with manual bInterval so we can force 1ms */
#define HID_DESC_HEAD(itf_num, str_idx, protocol, report_desc_len, ep_in, ep_out, ep_size) \
    9, TUSB_DESC_INTERFACE, itf_num, 0, 2,                    \
    TUSB_CLASS_HID, 0, protocol, str_idx,                     \
    9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1,       \
    HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(report_desc_len),     \
    7, TUSB_DESC_ENDPOINT, ep_in,  TUSB_XFER_INTERRUPT,       \
    U16_TO_U8S_LE(ep_size), 1,                                 \
    7, TUSB_DESC_ENDPOINT, ep_out, TUSB_XFER_INTERRUPT,       \
    U16_TO_U8S_LE(ep_size), 1

static uint8_t const config_descriptor[] = {
    /* Configuration */
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    /* HID interface with bInterval=1 on both endpoints */
    HID_DESC_HEAD(0, 0, HID_ITF_PROTOCOL_NONE,
                  sizeof(hid_report_descriptor),
                  EPNUM_HID_IN, EPNUM_HID_OUT, CFG_TUD_HID_EP_BUFSIZE),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return config_descriptor;
}

/* ──────────────────────────────────────────────────────────────────────────── */
/*  String descriptors                                                          */
/* ──────────────────────────────────────────────────────────────────────────── */
static char const *string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 }, /* 0: supported language = English */
    "Logitech",                    /* 1: Manufacturer */
    "Gaming Mouse G502",           /* 2: Product */
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
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
        const char *str = string_desc_arr[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++)
            _desc_str[1 + i] = str[i];
    }
    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
