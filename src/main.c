/*
 * main.c - PixelAssist Pico2 Mouse Bridge (Waveshare RP2350-USB-A)
 *
 * Architecture (mirrors the working Arduino firmware exactly):
 *   core0 : USB Device (native USB-C → PC)  — tud_task loop
 *   core1 : USB Host  (PIO GP12/GP13 → mouse) — pio_usb_host_frame + tuh_task
 *
 * Pin assignments confirmed from rp2350a/PixelAssist_RP2350A_src/:
 *   GP12 = USB-A D+   GP13 = USB-A D-   (Waveshare RP2350-USB-A onboard port)
 *   GP16 = WS2812 RGB LED
 *
 * Status LED colours:
 *   White  (500 ms at boot) = firmware started
 *   Blue   = PC has not enumerated this board
 *   Red    = PC OK, nothing on USB-A
 *   Yellow = USB-A device mounted, no reports yet
 *   Cyan   = motion parsed, not yet forwarded to PC
 *   White  = reports forwarded to PC
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/sync.h"
#include "tusb.h"
#include "pio_usb.h"
#include "hid_app.h"
#include "hid_device.h"
#include "dbg.h"

#define PIN_USB_HOST_DP  12   /* D+; D- is automatically DP+1 = GP13 */
#define PIN_RGB_LED      16   /* WS2812 on Waveshare RP2350-USB-A     */

/* Shared mouse state — written by core1 host task, read by core0 device task */
volatile mouse_report_t g_mouse       = {0};
volatile bool           g_mouse_dirty = false;

extern volatile uint32_t g_host_mounted;
extern volatile uint32_t g_host_reports;
extern volatile uint32_t g_host_motion;
extern volatile uint32_t g_tx_ok;

/* ── WS2812 (PIO2 – PIO0/PIO1 are used by Pico-PIO-USB) ───────────────── */
static const uint16_t ws_prog_instr[] = { 0x6221, 0x1123, 0x1400, 0xa442 };
static const pio_program_t ws_prog = {
    .instructions = ws_prog_instr, .length = 4, .origin = -1
};
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
    pio_sm_put_blocking(ws_pio, ws_sm,
        (((uint32_t)g << 16) | ((uint32_t)r << 8) | b) << 8);
}

/* ── Core1: USB Host (PIO-USB) ─────────────────────────────────────────── */
static void core1_entry(void)
{
    /*
     * Initialise PIO-USB host DIRECTLY (same as the Arduino working firmware).
     * We do NOT rely on tuh_configure because TinyUSB 2.1.0 only activates
     * hcd_pio_usb.c for OPT_MCU_RP2040, not OPT_MCU_RP2350.
     * PICO_PIO_USB_PIN_DP=12 is set at compile time so PIO_USB_DEFAULT_CONFIG
     * already has the right pin; we also set it explicitly for safety.
     */
    sleep_ms(20);   /* let core0 finish tud_init */

    pio_usb_configuration_t cfg = PIO_USB_DEFAULT_CONFIG;
    cfg.pin_dp = PIN_USB_HOST_DP;
    cfg.skip_alarm_pool = true;   /* we drive SOF frames ourselves below */

    /* Call tuh_configure so TinyUSB's hcd layer records the config (belt+braces) */
    tuh_configure(CFG_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &cfg);

    /* Also initialise PIO-USB directly — this is what the Arduino does */
    pio_usb_host_init(&cfg);

    /* Now bring up the TinyUSB host stack */
    tuh_init(CFG_TUH_RHPORT);

    dbg_printf("core1: PIO-USB host init done, pin_dp=%d\r\n", cfg.pin_dp);

    /* Drive 1 ms SOF frames + run the host stack */
    uint64_t next_frame = 0;
    while (true) {
        uint64_t now = time_us_64();
        if (next_frame == 0) next_frame = now;
        if ((int64_t)(now - next_frame) >= 0) {
            pio_usb_host_frame();
            next_frame += 1000;
            /* Prevent runaway if we fall behind */
            if ((int64_t)(time_us_64() - next_frame) > 5000)
                next_frame = time_us_64();
        }
        tuh_task();
    }
}

/* ── LED task (core0) ──────────────────────────────────────────────────── */
static void led_task(void)
{
    static uint32_t t0 = 0;
    static uint32_t last_rep = 0, last_rep_t = 0;
    static uint32_t last_mot = 0, last_mot_t = 0;
    static uint32_t last_tx  = 0, last_tx_t  = 0;
    static int last_state = -1;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - t0 < 50) return;
    t0 = now;

    uint32_t r = g_host_reports, m = g_host_motion, x = g_tx_ok;
    if (r != last_rep) { last_rep = r; last_rep_t = now; }
    if (m != last_mot) { last_mot = m; last_mot_t = now; }
    if (x != last_tx)  { last_tx  = x; last_tx_t  = now; }

    int state;
    if (!tud_mounted())                        state = 0;   /* blue   */
    else if (x && now - last_tx_t  < 400)      state = 5;   /* white  */
    else if (m && now - last_mot_t < 400)      state = 4;   /* cyan   */
    else if (r && now - last_rep_t < 400)      state = 3;   /* green  */
    else if (g_host_mounted)                   state = 2;   /* yellow */
    else                                       state = 1;   /* red    */

    if (state == last_state) return;
    last_state = state;
    switch (state) {
        case 0: rgb_set(0,  0,  40); break;   /* blue   */
        case 1: rgb_set(40, 0,  0);  break;   /* red    */
        case 2: rgb_set(40, 28, 0);  break;   /* yellow */
        case 3: rgb_set(0,  40, 0);  break;   /* green  */
        case 4: rgb_set(0,  30, 30); break;   /* cyan   */
        default: rgb_set(40, 40, 40); break;  /* white  */
    }
}

/* ── Core0: USB Device + main loop ────────────────────────────────────── */
int main(void)
{
    /* 120 MHz: valid for PIO-USB, safe at default RP2350 core voltage */
    set_sys_clock_khz(120000, true);

    rgb_init();
    rgb_set(40, 40, 40);   /* white = firmware started */
    sleep_ms(500);

    tud_init(0);            /* device on native USB (rhport 0) */

    multicore_launch_core1(core1_entry);

    static uint32_t stat_t = 0;
    while (true) {
        tud_task();
        hid_device_task();
        led_task();
        dbg_task();

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - stat_t >= 1000) {
            stat_t = now;
            dbg_printf("STAT mounted=%lu reports=%lu motion=%lu txok=%lu pcmounted=%d\r\n",
                       (unsigned long)g_host_mounted,
                       (unsigned long)g_host_reports,
                       (unsigned long)g_host_motion,
                       (unsigned long)g_tx_ok,
                       tud_mounted());
        }
    }
}
