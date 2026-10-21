/*
 * main.c  -  PixelAssist Pico2 Mouse Bridge
 * ==========================================
 * RP2350A has TWO native USB PHYs:
 *   USB0 (RHPORT 0) = Device -> PC   (HID composite, 1000 Hz, Report IDs 1/2/3)
 *   USB1 (RHPORT 1) = Host   -> G502 (reads mouse, forwards as Report ID 1)
 *
 * Core 1 runs the USB device task.
 * Core 0 runs the USB host task.
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "hid_app.h"
#include "hid_device.h"

/* Core 1: USB device engine (PC-facing) */
static void core1_entry(void)
{
    /* Init device port (RHPORT 0) on core1 */
    tusb_rhport_init_t dev_init = {
        .role  = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO,
    };
    tusb_init(CFG_TUD_RHPORT, &dev_init);

    while (true) {
        tud_task();
        hid_device_task();
    }
}

int main(void)
{
    board_init();

    /* Init host port (RHPORT 1) on core0 */
    tusb_rhport_init_t host_init = {
        .role  = TUSB_ROLE_HOST,
        .speed = TUSB_SPEED_AUTO,
    };
    tusb_init(CFG_TUH_RHPORT, &host_init);

    multicore_launch_core1(core1_entry);

    /* Core 0: USB host engine (mouse-facing) */
    while (true) {
        tuh_task();
        hid_app_task();
    }
}
