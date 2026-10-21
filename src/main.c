/*
 * main.c  -  PixelAssist Pico2 Mouse Bridge
 * ==========================================
 * RP2350A has ONE native USB PHY:
 *   Native USB  = Device -> PC   (composite HID, 1000 Hz)
 *   pio-usb GP2 = Host   -> G502 (reads mouse, re-emits as HID report)
 *
 * TinyUSB hcd_rp2040.c calls pio_usb_host_init() internally when
 * CFG_TUH_RPI_PIO_USB=1 and PICO_PIO_USB_PIN_DP is set.
 *
 * Core 1: tud_task (device)
 * Core 0: tuh_task (host)
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "hid_app.h"
#include "hid_device.h"

/* Shared mouse state: written by core0 host task, read by core1 device task */
volatile mouse_report_t g_mouse       = {0};
volatile bool           g_mouse_dirty = false;

/* Core 1: USB device engine (native USB -> PC) */
static void core1_entry(void)
{
    tusb_init();    /* inits device on rhport 0 (native USB) */
    while (true) {
        tud_task();
        hid_device_task();
    }
}

int main(void)
{
    board_init();
    multicore_launch_core1(core1_entry);

    /* Small delay so core1 tusb_init completes before host starts */
    sleep_ms(10);

    /* Init host stack on rhport 1 (pio-usb, pin_dp=2 set in tusb_config.h) */
    tuh_init(CFG_TUH_RHPORT);

    while (true) {
        tuh_task();
        hid_app_task();
    }
}
