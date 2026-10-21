/*
 * tusb_config.h
 * =============
 * TinyUSB configuration for RP2350A dual-role firmware.
 *
 * Device port  : native USB phy (USB port connected to PC)
 * Host port    : pio-usb on GPIO 2/3 (D+/D-) - second USB port for mouse
 *                OR the second USB phy if using a board that has one.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ── Common ─────────────────────────────────────────────────────────────── */
#define CFG_TUSB_MCU               OPT_MCU_RP2350
#define CFG_TUSB_OS                OPT_OS_PICO
#define CFG_TUSB_DEBUG             0

/* ── Device (PC-facing) ──────────────────────────────────────────────────── */
#define CFG_TUD_ENABLED            1

/* One HID interface: composite mouse + keyboard + vendor */
#define CFG_TUD_HID                1
#define CFG_TUD_HID_EP_BUFSIZE     16   /* max HID packet size (full-speed) */
#define CFG_TUD_ENDPOINT0_SIZE     64

/* ── Host (mouse-facing, pio-usb) ────────────────────────────────────────── */
#define CFG_TUH_ENABLED            1
#define CFG_TUH_MAX_SPEED          OPT_MODE_FULL_SPEED

/* We only need one hub, one HID mouse */
#define CFG_TUH_HUB                1
#define CFG_TUH_HID                4   /* up to 4 HID instances on host     */
#define CFG_TUH_HID_EPIN_BUFSIZE   64
#define CFG_TUH_HID_EPOUT_BUFSIZE  64

/* ── pio-usb pin assignment (second port) ───────────────────────────────── */
/* D+ on GP2, D- on GP3.  Adjust to match your board's second USB connector */
#define PICO_PIO_USB_PIN_DP        2

#ifdef __cplusplus
}
#endif
