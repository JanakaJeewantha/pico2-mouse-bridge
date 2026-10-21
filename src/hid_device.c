/*
 * hid_device.c  -  USB Device (PC-facing) side
 *
 * Composite HID on RHPORT 0, three top-level collections:
 *   Report ID 1  IN  4 B  - Mouse  (buttons, X, Y, wheel)
 *   Report ID 3  IN  8 B  - Boot Keyboard
 *   Report ID 2  OUT 7 B  - Vendor command channel
 */

#include "hid_device.h"
#include "vk_to_hid.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"      /* save_and_disable_interrupts / restore */
#include "pico/bootrom.h"       /* reset_usb_boot                         */
#include <string.h>

/* ── Keyboard state ──────────────────────────────────────────────────────── */
static uint8_t kb_report[8];
static bool    kb_dirty = false;

/* ── Mouse accumulators (host core adds, device core drains) ──────────────── */
static volatile int32_t acc_x = 0, acc_y = 0, acc_w = 0;
static volatile uint8_t cur_btn = 0, last_btn = 0;

static inline int32_t clamp127(int32_t v) { return v > 127 ? 127 : (v < -127 ? -127 : v); }

/* ── Forward passthrough from host mouse ─────────────────────────────────── */
void hid_device_task(void)
{
    if (!tud_hid_ready()) return;

    /* 1. Forward physical mouse from host */
    int32_t px = acc_x, py = acc_y, pw = acc_w;
    uint8_t btn = cur_btn;
    if (px || py || pw || btn != last_btn) {
        int32_t cx = clamp127(px), cy = clamp127(py), cw = clamp127(pw);
        __atomic_fetch_sub(&acc_x, cx, __ATOMIC_SEQ_CST);
        __atomic_fetch_sub(&acc_y, cy, __ATOMIC_SEQ_CST);
        __atomic_fetch_sub(&acc_w, cw, __ATOMIC_SEQ_CST);
        last_btn = btn;
        uint8_t rpt[4] = { btn, (uint8_t)(int8_t)cx, (uint8_t)(int8_t)cy, (uint8_t)(int8_t)cw };
        tud_hid_report(1, rpt, sizeof(rpt));
        return;
    }

    /* 2. Send keyboard if changed */
    if (kb_dirty) {
        kb_dirty = false;
        tud_hid_report(3, kb_report, sizeof(kb_report));
    }
}

/* Called from the host core: accumulate motion, never overwrite */
void hid_device_send_mouse(uint8_t buttons, int32_t x, int32_t y, int32_t wheel)
{
    cur_btn = buttons;
    __atomic_fetch_add(&acc_x, x, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&acc_y, y, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&acc_w, wheel, __ATOMIC_SEQ_CST);
}

/* PC app aim override packet (0xAA) */
void hid_device_send_aim(int16_t dx, int16_t dy)
{
    __atomic_fetch_add(&acc_x, dx, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&acc_y, dy, __ATOMIC_SEQ_CST);
}

/* Key press/release (0xAB) */
void hid_device_key_event(uint8_t hid_keycode, uint8_t modifier_mask, bool down)
{
    if (modifier_mask) {
        if (down) kb_report[0] |=  modifier_mask;
        else      kb_report[0] &= ~modifier_mask;
        kb_dirty = true;
    }
    if (hid_keycode != 0) {
        if (down) {
            for (int i = 2; i < 8; i++) {
                if (kb_report[i] == 0) { kb_report[i] = hid_keycode; break; }
            }
        } else {
            for (int i = 2; i < 8; i++) {
                if (kb_report[i] == hid_keycode) { kb_report[i] = 0; break; }
            }
        }
        kb_dirty = true;
    }
    if (kb_dirty && tud_hid_ready()) {
        kb_dirty = false;
        tud_hid_report(3, kb_report, sizeof(kb_report));
    }
}

/* ── TinyUSB device callbacks ────────────────────────────────────────────── */
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen)
{
    (void)itf; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize)
{
    (void)itf; (void)report_type;
    if (report_id != 2 || bufsize < 7) return;

    uint8_t hdr = buffer[0];

    if (hdr == 0xAA) {
        uint8_t chk = buffer[1] ^ buffer[2] ^ buffer[3] ^ buffer[4] ^ buffer[5];
        if (chk != buffer[6]) return;
        int16_t dx = (int16_t)(buffer[1] | ((uint16_t)buffer[2] << 8));
        int16_t dy = (int16_t)(buffer[3] | ((uint16_t)buffer[4] << 8));
        if ((buffer[5] & 0x01) && (dx != 0 || dy != 0))
            hid_device_send_aim(dx, dy);

    } else if (hdr == 0xAB) {
        uint8_t vk   = buffer[1];
        bool    down = (buffer[2] != 0);
        if ((vk ^ buffer[2]) != buffer[6]) return;
        uint8_t hid_kc = 0, mod = 0;
        vk_to_hid(vk, &hid_kc, &mod);
        hid_device_key_event(hid_kc, mod, down);

    } else if (hdr == 0xFF) {
        reset_usb_boot(0, 0);   /* reboot into BOOTSEL mass-storage DFU */
    }
}
