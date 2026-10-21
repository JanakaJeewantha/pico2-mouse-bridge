/*
 * hid_app.c
 * =========
 * TinyUSB HOST callback implementations.
 * When a USB HID mouse is plugged into the second port the host stack
 * enumerates it and calls tuh_hid_report_received_cb() every time the
 * mouse sends a new report.  We extract buttons/X/Y/wheel and forward
 * them to the PC via hid_device_send_mouse().
 *
 * Supports boot-protocol mice (3-byte) and typical 4/5-byte reports.
 */

#include "hid_app.h"
#include "hid_device.h"
#include "tusb.h"
#include <string.h>

/* Called periodically from core0 main loop */
void hid_app_task(void)
{
    /* Nothing to poll - TinyUSB host is interrupt/callback driven */
}

/* -- TinyUSB host HID callbacks -------------------------------------------- */

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const *desc_report, uint16_t desc_len)
{
    (void)desc_report; (void)desc_len;
    /* Request boot-protocol for mice so we get a predictable report layout */
    uint8_t const itf_protocol = tuh_hid_get_protocol(dev_addr, instance);
    if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
        tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_BOOT);
    }
    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    (void)dev_addr; (void)instance;
    /* Mouse disconnected - send a "all buttons up, no motion" report */
    hid_device_send_mouse(0, 0, 0, 0);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                 uint8_t const *report, uint16_t len)
{
    uint8_t const itf_protocol = tuh_hid_get_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
        /* Boot protocol: [0]=buttons [1]=X [2]=Y  (optional [3]=wheel) */
        if (len >= 3) {
            uint8_t buttons = report[0] & 0x07; /* LMB/RMB/MMB       */
            int8_t  x       = (int8_t)report[1];
            int8_t  y       = (int8_t)report[2];
            int8_t  wheel   = (len >= 4) ? (int8_t)report[3] : 0;
            hid_device_send_mouse(buttons, x, y, wheel);
        }
    }

    /* Re-arm to receive next report */
    tuh_hid_receive_report(dev_addr, instance);
}
