/*
 * main.c - PixelAssist Pico2 Mouse Bridge (Waveshare RP2350-USB-A)
 *   core0 : USB device (native USB-C -> PC)
 *   core1 : PIO-USB host on GP12/GP13 (USB-A port), 1 ms frames driven manually
 *
 * Status LED = onboard WS2812 RGB on GP16 (driven from PIO2):
 *   BLUE    PC has not enumerated this board yet
 *   RED     PC OK, nothing detected on the USB-A port
 *   YELLOW  USB-A device mounted, no reports yet
 *   GREEN   mouse reports flowing
 */
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "tusb.h"
#include "pio_usb.h"
#include "hid_app.h"
#include "hid_device.h"
#include "dbg.h"

#define PIN_USB_HOST_DP 12
#define PIN_RGB_LED     16

volatile mouse_report_t g_mouse       = {0};
volatile bool           g_mouse_dirty = false;

extern volatile uint32_t g_host_mounted;
extern volatile uint32_t g_host_reports;
extern volatile uint32_t g_host_motion;
extern volatile uint32_t g_tx_ok;

/* ---- WS2812 on PIO2 (PIO-USB uses PIO0/PIO1) ---- */
static const uint16_t ws_prog_instr[] = { 0x6221, 0x1123, 0x1400, 0xa442 };
static const pio_program_t ws_prog = { .instructions = ws_prog_instr, .length = 4, .origin = -1 };
static PIO ws_pio; static uint ws_sm;

static void rgb_init(void)
{
    ws_pio = pio2;
    ws_sm  = pio_claim_unused_sm(ws_pio, true);
    uint off = pio_add_program(ws_pio, &ws_prog);
    pio_gpio_init(ws_pio, PIN_RGB_LED);
    pio_sm_set_consecutive_pindirs(ws_pio, ws_sm, PIN_RGB_LED, 1, true);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, off, off + 3);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, PIN_RGB_LED);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (800000.0f * 10.0f));
    pio_sm_init(ws_pio, ws_sm, off, &c);
    pio_sm_set_enabled(ws_pio, ws_sm, true);
}

static void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    pio_sm_put_blocking(ws_pio, ws_sm, (((uint32_t)g << 16) | ((uint32_t)r << 8) | b) << 8);
}

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
    static uint32_t t0 = 0, last_rep = 0, last_rep_t = 0, last_mot = 0, last_mot_t = 0, last_tx = 0, last_tx_t = 0;
    static int last_state = -1;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - t0 < 50) return;
    t0 = now;
    uint32_t r = g_host_reports, m = g_host_motion, x = g_tx_ok;
    if (r != last_rep) { last_rep = r; last_rep_t = now; }
    if (m != last_mot) { last_mot = m; last_mot_t = now; }
    if (x != last_tx)  { last_tx = x;  last_tx_t = now; }

    int state;
    if (!tud_mounted())                         state = 0;
    else if (x && now - last_tx_t < 300)        state = 5;
    else if (m && now - last_mot_t < 300)       state = 4;
    else if (r && now - last_rep_t < 300)       state = 3;
    else if (g_host_mounted)                    state = 2;
    else                                        state = 1;
    if (state == last_state) return;
    last_state = state;
    switch (state) {
        case 0: rgb_set(0, 0, 40);  break;   /* blue    */
        case 1: rgb_set(40, 0, 0);  break;   /* red     */
        case 2: rgb_set(40, 28, 0); break;   /* yellow  */
        case 3: rgb_set(0, 40, 0);  break;   /* green   */
        case 4: rgb_set(0, 30, 30); break;   /* cyan    */
        default: rgb_set(40, 40, 40); break; /* white   */
    }
}

int main(void)
{
    /* PIO-USB needs 120/240 MHz. 120 is safe at default core voltage. */
    set_sys_clock_khz(120000, true);

    rgb_init();
    rgb_set(40, 40, 40);                 /* white = firmware booted */
    sleep_ms(500);

    tud_init(0);                         /* device on native USB */
    multicore_launch_core1(core1_entry);

    while (true) {
        tud_task();
        hid_device_task();
        led_task();
        dbg_task();
        static uint32_t ts = 0;
        uint32_t nw = to_ms_since_boot(get_absolute_time());
        if (nw - ts >= 1000) {
            ts = nw;
            dbg_printf("STAT mounted=%lu reports=%lu motion=%lu txok=%lu pcmounted=%d\r\n",
                       (unsigned long)g_host_mounted, (unsigned long)g_host_reports,
                       (unsigned long)g_host_motion, (unsigned long)g_tx_ok, tud_mounted());
        }
    }
}
