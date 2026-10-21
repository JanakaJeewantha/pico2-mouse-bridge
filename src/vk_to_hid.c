/*
 * vk_to_hid.c
 * ===========
 * Lightweight Windows Virtual-Key -> HID keycode + modifier-mask lookup.
 * Only what is needed for PixelAssist key injection (NeoStrafe / BHop macros).
 *
 * modifier_mask bits (HID modifier byte):
 *   0 = LCTRL   1 = LSHIFT  2 = LALT   3 = LGUI
 *   4 = RCTRL   5 = RSHIFT  6 = RALT   7 = RGUI
 */

#include <stdint.h>
#include <stdbool.h>

void vk_to_hid(uint8_t vk, uint8_t *hid_kc, uint8_t *mod)
{
    *hid_kc = 0;
    *mod    = 0;

    /* Letters A-Z: VK 0x41-0x5A -> HID 0x04-0x1D */
    if (vk >= 0x41 && vk <= 0x5A) { *hid_kc = 0x04 + (vk - 0x41); return; }

    /* Digits 1-9: VK 0x31-0x39 -> HID 0x1E-0x26 */
    if (vk >= 0x31 && vk <= 0x39) { *hid_kc = 0x1E + (vk - 0x31); return; }
    if (vk == 0x30) { *hid_kc = 0x27; return; }  /* 0 */

    /* F1-F12 */
    if (vk >= 0x70 && vk <= 0x7B) { *hid_kc = 0x3A + (vk - 0x70); return; }

    switch (vk) {
        /* Common keys */
        case 0x20: *hid_kc = 0x2C; return; /* SPACE     */
        case 0x0D: *hid_kc = 0x28; return; /* ENTER     */
        case 0x1B: *hid_kc = 0x29; return; /* ESCAPE    */
        case 0x08: *hid_kc = 0x2A; return; /* BACKSPACE */
        case 0x09: *hid_kc = 0x2B; return; /* TAB       */
        /* Arrow keys */
        case 0x25: *hid_kc = 0x50; return; /* LEFT      */
        case 0x27: *hid_kc = 0x4F; return; /* RIGHT     */
        case 0x26: *hid_kc = 0x52; return; /* UP        */
        case 0x28: *hid_kc = 0x51; return; /* DOWN      */
        /* Navigation */
        case 0x2D: *hid_kc = 0x49; return; /* INSERT    */
        case 0x2E: *hid_kc = 0x4C; return; /* DELETE    */
        case 0x24: *hid_kc = 0x4A; return; /* HOME      */
        case 0x23: *hid_kc = 0x4D; return; /* END       */
        case 0x21: *hid_kc = 0x4B; return; /* PGUP      */
        case 0x22: *hid_kc = 0x4E; return; /* PGDN      */
        /* Modifiers - only set mask, kc stays 0 */
        case 0xA2: *mod = (1<<0); return;  /* LCTRL     */
        case 0xA3: *mod = (1<<4); return;  /* RCTRL     */
        case 0xA0: *mod = (1<<1); return;  /* LSHIFT    */
        case 0xA1: *mod = (1<<5); return;  /* RSHIFT    */
        case 0xA4: *mod = (1<<2); return;  /* LALT      */
        case 0xA5: *mod = (1<<6); return;  /* RALT      */
        case 0x5B: *mod = (1<<3); return;  /* LWIN      */
        case 0x5C: *mod = (1<<7); return;  /* RWIN      */
        /* Punctuation */
        case 0xBD: *hid_kc = 0x2D; return; /* - OEM_MINUS  */
        case 0xBB: *hid_kc = 0x2E; return; /* = OEM_PLUS   */
        case 0xDB: *hid_kc = 0x2F; return; /* [            */
        case 0xDD: *hid_kc = 0x30; return; /* ]            */
        case 0xDC: *hid_kc = 0x31; return; /* backslash    */
        case 0xBA: *hid_kc = 0x33; return; /* ;            */
        case 0xDE: *hid_kc = 0x34; return; /* apostr.      */
        case 0xC0: *hid_kc = 0x35; return; /* `            */
        case 0xBC: *hid_kc = 0x36; return; /* ,            */
        case 0xBE: *hid_kc = 0x37; return; /* .            */
        case 0xBF: *hid_kc = 0x38; return; /* /            */
        default: return; /* unknown */
    }
}
