/*
 * main.c - PixelAssist Pico2 Mouse Bridge
 * Mirrors the layout of the working Arduino firmware:
 *   core0 : USB device (native USB -> PC), 240 MHz
 *   core1 : PIO-USB host on GP12/GP13 (USB-A port), frames driven manually
 * LED: 3 blinks at boot; blink = device mounted on USB-A; solid = reports flowing.
 */
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "pio_usb.h"
#include "hid_app.h"
#include "hid_device.h"

#define PIN_USB_HOST_DP 12

#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif

volatile mouse_report_t g_mouse       = {0};
volatile bool           g_mouse_dirty = false;

extern volatile uint32_t g_host_mounted;
extern volatile uint32_t g_host_reports;

static void core1_entry(void)
{
    sleep_ms(20);
    pio_usb_configuration_t cfg = PIO_USB_DEFAULT_CONFIG;
    cfg.pin_dp = PIN_USB_HOST_DP;
    cfg.skip_alarm_pool = true;          /* we drive 1 ms frames ourselves */
    tuh_configure(CFG_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &cfg);
    tuh_init(CFG_TUH_RHPORT);

    uint64_t next = 0;
    while (true) {
        uint64_t now = time_us_64();
        if (next == 0) next = now;
        if ((int64_t)(now - next) >= 0) {
            pio_usb_host_frame();
            next += 1000;
            if ((int64_t)(time_us_64() - next) > 5000) next = time_us_64();
        }
        tuh_task();
    }
}

static void led_task(void)
{
    static uint32_t t0 = 0, last_rep = 0, last_rep_t = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - t0 < 50) return;
    t0 = now;
    uint32_t r = g_host_reports;
    if (r != last_rep) { last_rep = r; last_rep_t = now; }
    bool on;
    if (now - last_rep_t < 200 && r) on = true;                 /* reports flowing */
    else if (g_host_mounted)         on = (now / 250) & 1;      /* mounted, idle   */
    else                             on = false;                /* nothing on USB-A*/
    gpio_put(PICO_DEFAULT_LED_PIN, on);
}

int main(void)
{
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    for (int i = 0; i < 3; i++) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1); sleep_ms(80);
        gpio_put(PICO_DEFAULT_LED_PIN, 0); sleep_ms(80);
    }

    /* PIO-USB needs 120/240 MHz. 120 is safe at default core voltage. */
    set_sys_clock_khz(120000, true);
    gpio_put(PICO_DEFAULT_LED_PIN, 1); sleep_ms(300);   /* 4th long blink = clock OK */
    gpio_put(PICO_DEFAULT_LED_PIN, 0);

    tud_init(0);                         /* device on native USB */
    multicore_launch_core1(core1_entry);

    while (true) {
        tud_task();
        hid_device_task();
        led_task();
    }
}
