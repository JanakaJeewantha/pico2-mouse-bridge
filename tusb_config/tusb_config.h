/*
 * tusb_config.h  -  TinyUSB configuration for RP2350A
 *
 * RP2350A has TWO native full-speed USB PHYs:
 *   RHPORT 0 (USB0) -> Device mode -> PC (HID composite, 1000 Hz)
 *   RHPORT 1 (USB1) -> Host mode   -> G502 mouse
 *
 * No pio-usb / bit-bang needed.
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* ── Common ─────────────────────────────────────────────────────────────── */
#define CFG_TUSB_MCU        OPT_MCU_RP2350
#define CFG_TUSB_OS         OPT_OS_PICO
#define CFG_TUSB_DEBUG      0
#define CFG_TUSB_MEM_ALIGN  TU_ATTR_ALIGNED(4)

/* ── Device  (USB0, RHPORT 0, PC-facing) ───────────────────────────────── */
#define CFG_TUD_ENABLED         1
#define CFG_TUD_RHPORT          0
#define CFG_TUD_ENDPOINT0_SIZE  64
#define CFG_TUD_HID             1
#define CFG_TUD_HID_EP_BUFSIZE  16
#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0

/* ── Host  (USB1, RHPORT 1, mouse-facing) ───────────────────────────────── */
#define CFG_TUH_ENABLED         1
#define CFG_TUH_RHPORT          1
#define CFG_TUH_MAX_SPEED       OPT_MODE_FULL_SPEED
#define CFG_TUH_HUB             1
#define CFG_TUH_HID             4
#define CFG_TUH_HID_EPIN_BUFSIZE   64
#define CFG_TUH_HID_EPOUT_BUFSIZE  64
#define CFG_TUH_CDC             0
#define CFG_TUH_MSC             0

#ifdef __cplusplus
}
#endif
