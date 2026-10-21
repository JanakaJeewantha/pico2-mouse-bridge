/*
 * tusb_config.h
 * =============
 * DO NOT define CFG_TUSB_MCU here - pico-sdk sets it via compiler -D flag.
 *
 * RP2350A: ONE native USB PHY
 *   RHPORT 0 = native USB = Device (-> PC, 1000 Hz HID)
 *   RHPORT 1 = pio-usb    = Host   (-> G502 mouse, GP2/GP3)
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* OS integration */
#define CFG_TUSB_OS             OPT_OS_PICO
#define CFG_TUSB_DEBUG          0
#define CFG_TUSB_MEM_ALIGN      TU_ATTR_ALIGNED(4)

/* ── Device (native USB, RHPORT 0, -> PC) ───────────────────────────────── */
#define CFG_TUD_ENABLED         1
#define CFG_TUD_RHPORT          0
#define CFG_TUD_ENDPOINT0_SIZE  64

#define CFG_TUD_HID             1
#define CFG_TUD_HID_EP_BUFSIZE  16

#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0

/* ── Host (pio-usb, RHPORT 1, GP2=D+/GP3=D-) ───────────────────────────── */
#define CFG_TUH_ENABLED             1
#define CFG_TUH_RHPORT              1
#define CFG_TUH_RPI_PIO_USB         1       /* use Pico-PIO-USB as HCD        */
#define PICO_PIO_USB_PIN_DP         2       /* D+ on GP2, D- auto = GP3       */

#define CFG_TUH_HUB                 1
#define CFG_TUH_HID                 4
#define CFG_TUH_HID_EPIN_BUFSIZE    64
#define CFG_TUH_HID_EPOUT_BUFSIZE   64

#define CFG_TUH_CDC                 0
#define CFG_TUH_MSC                 0

#ifdef __cplusplus
}
#endif
