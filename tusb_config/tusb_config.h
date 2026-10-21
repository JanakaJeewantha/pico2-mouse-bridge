/*
 * tusb_config.h  -  TinyUSB configuration for RP2350A dual-role firmware
 *
 * Device port : native USB PHY  (USB connector -> PC)
 * Host port   : pio-usb on GP2/GP3 (second connector -> G502 mouse)
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ── Board / OS ─────────────────────────────────────────────────────────── */
#define CFG_TUSB_MCU            OPT_MCU_RP2350
#define CFG_TUSB_OS             OPT_OS_PICO
#define CFG_TUSB_DEBUG          0
#define CFG_TUSB_MEM_SECTION    /* default */
#define CFG_TUSB_MEM_ALIGN      TU_ATTR_ALIGNED(4)

/* ── Device (PC-facing, native USB PHY) ─────────────────────────────────── */
#define CFG_TUD_ENABLED         1
#define CFG_TUD_ENDPOINT0_SIZE  64

/* One composite HID interface */
#define CFG_TUD_HID             1
#define CFG_TUD_HID_EP_BUFSIZE  16

/* No CDC, no MSC, no MIDI */
#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0

/* ── Host (mouse-facing, pio-usb on GP2/GP3) ────────────────────────────── */
#define CFG_TUH_ENABLED         1
#define CFG_TUH_MAX_SPEED       OPT_MODE_FULL_SPEED

/* Use Pico PIO as the host controller (bit-bang USB on GP2/GP3) */
#define CFG_TUH_RPI_PIO_USB     1
#define PICO_PIO_USB_PIN_DP     2   /* D+ on GP2 ; D- is automatically GP3 */

/* Enough for one hub + mouse */
#define CFG_TUH_HUB             1
#define CFG_TUH_HID             4
#define CFG_TUH_HID_EPIN_BUFSIZE  64
#define CFG_TUH_HID_EPOUT_BUFSIZE 64

/* No host CDC / MSC needed */
#define CFG_TUH_CDC             0
#define CFG_TUH_MSC             0

#ifdef __cplusplus
}
#endif
