/*
 * PixelAssist Pico2 Mouse Bridge - main.c
 * =========================================
 * Target : RP2350A (Pico 2 / Waveshare RP2350A)
 * SDK    : pico-sdk >= 2.0
 * USB lib: TinyUSB (bundled with pico-sdk)
 *
 * Device side (USB port -> PC):
 *   Composite HID: THREE top-level collections
 *   Endpoint bInterval = 1  -> 1000 Hz full-speed
 *   Report ID 1  (IN,  4 B) - Mouse    (buttons + X + Y + wheel)
 *   Report ID 3  (IN,  8 B) - Keyboard (modifier + reserved + 6 keycodes)
 *   Report ID 2  (OUT, 7 B) - Vendor cmd channel (aim / key-inject / update)
 *
 * Host side (second USB port <- physical G502 or any USB HID mouse):
 *   TinyUSB host reads mouse reports and re-emits them on device Report ID 1.
 *   The board is transparent - the PC sees ONE mouse (its own PID/VID).
 *
 * Command packets (Report ID 2, received from PC app):
 *   [0]=0xAA  Aim override   [1..2]=DX s16LE [3..4]=DY s16LE
 *                            [5]=flags(0x01=tracking) [6]=XOR chk
 *   [0]=0xAB  Key inject     [1]=VK  [2]=down  [6]=VK^down
 *   [0]=0xFF  Reboot to BOOTSEL
 */

#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "bsp/board.h"
#include "tusb.h"
#include "hid_app.h"
#include "hid_device.h"

/* shared state written by host task, read by device task */
volatile mouse_report_t g_mouse       = {0};
volatile bool           g_mouse_dirty = false;

/* Core 1: USB device engine */
static void core1_entry(void)
{
    while (true) {
        tud_task();
        hid_device_task();
    }
}

int main(void)
{
    board_init();
    tusb_init();
    multicore_launch_core1(core1_entry);

    /* Core 0: USB host engine */
    while (true) {
        tuh_task();
        hid_app_task();
    }
}
