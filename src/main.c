/*
 * main.c  -  PixelAssist Pico2 Mouse Bridge
 * ==========================================
 * RP2350A has TWO native USB PHYs:
 *   USB0 (RHPORT 0) = Device -> PC   (HID composite, 1000 Hz)
 *   USB1 (RHPORT 1) = Host   -> G502 (reads mouse, forwards as Report ID 1)
 *
 * tusb_config.h tells TinyUSB which rhport is device and which is host.
 * Core 1 runs tud_task(). Core 0 runs tuh_task().
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "hid_app.h"
#include "hid_device.h"

/* Core 1: USB device engine (PC-facing, RHPORT 0) */
static void core1_entry(void)
{
    while (true) {
        tud_task();
        hid_device_task();
    }
}

int main(void)
{
    board_init();   /* board_init() calls tusb_init() internally via BSP  */
    /* If board_init doesn't call tusb_init, call it explicitly:         */
    tusb_init();

    multicore_launch_core1(core1_entry);

    /* Core 0: USB host engine (mouse-facing, RHPORT 1) */
    while (true) {
        tuh_task();
        hid_app_task();
    }
}
