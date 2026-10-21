/*
 * main.c  -  PixelAssist Pico2 Mouse Bridge
 * ==========================================
 * RP2350A / Pico2 has ONE native USB PHY:
 *   Native USB = Device port -> PC  (HID composite, 1000 Hz)
 *   PIO USB    = Host port   -> G502 mouse (pio-usb on GP2/GP3)
 *
 * Core 1: USB device task (tud_task)
 * Core 0: USB host task   (tuh_task via pio-usb)
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "bsp/board_api.h"
#include "pio_usb.h"
#include "tusb.h"
#include "hid_app.h"
#include "hid_device.h"

/* Shared mouse state – written by core0 host task, read by core1 device task */
volatile mouse_report_t g_mouse       = {0};
volatile bool           g_mouse_dirty = false;

/* Core 1: USB device engine (native USB, PC-facing) */
static void core1_entry(void)
{
    /* Device stack runs on native USB (no init args needed in tusb) */
    tusb_init();

    while (true) {
        tud_task();
        hid_device_task();
    }
}

int main(void)
{
    board_init();

    /* PIO-USB host on GP2/GP3 – must be called BEFORE multicore launch */
    static pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = 2;   /* D+ on GP2, D- auto = GP3 */
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB, &pio_cfg);

    /* Init host stack on rhport 1 (pio-usb) */
    tuh_init(1);

    multicore_launch_core1(core1_entry);

    /* Core 0: USB host engine */
    while (true) {
        tuh_task();
        hid_app_task();
    }
}
