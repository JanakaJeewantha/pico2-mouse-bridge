/*
 * hid_app.c - TinyUSB HOST HID callbacks (runs on core1).
 * Port of the working Arduino firmware logic: parse the mouse report
 * descriptor (report IDs, 8/16-bit X/Y), fall back to boot protocol.
 */
#include "hid_app.h"
#include "hid_device.h"
#include "tusb.h"
#include <string.h>
#include "dbg.h"

volatile uint32_t g_host_mounted = 0;   /* LED: any device mounted    */
volatile uint32_t g_host_reports = 0;   /* LED: reports received      */
volatile uint32_t g_host_motion  = 0;   /* LED: nonzero motion parsed */

typedef struct {
    bool    valid;
    uint8_t id;
    int16_t btnOff; uint8_t btnCnt;
    int16_t xOff, yOff, wOff;
    uint8_t xSz, ySz, wSz;
} mouse_map_t;

#define MAXD 8
static mouse_map_t maps[MAXD][MAXD];
static bool boot_mouse[MAXD][MAXD];

static void map_clear(mouse_map_t *m)
{
    memset(m, 0, sizeof(*m));
    m->btnOff = m->xOff = m->yOff = m->wOff = -1;
}

void hid_app_task(void) { }

static bool parse_mouse(const uint8_t *d, uint16_t len, mouse_map_t *out)
{
    uint16_t usagePage = 0; uint32_t rSize = 0, rCount = 0; uint8_t rid = 0;
    uint32_t usages[24]; uint8_t nUsages = 0; uint32_t uMin = 0; bool haveMin = false;
    uint16_t offs[16] = {0};
    mouse_map_t byId[16];
    for (int k = 0; k < 16; k++) map_clear(&byId[k]);
    uint16_t i = 0;

    while (i < len) {
        uint8_t b = d[i++];
        if (b == 0xFE) { if (i >= len) break; i += 2 + d[i]; continue; }
        uint8_t sz = b & 3; if (sz == 3) sz = 4;
        uint8_t type = (b >> 2) & 3, tag = b >> 4;
        if (i + sz > len) break;
        uint32_t v = 0;
        for (uint8_t k = 0; k < sz; k++) v |= (uint32_t)d[i + k] << (8 * k);
        i += sz;

        if (type == 1) {
            switch (tag) {
                case 0: usagePage = v & 0xFFFF; break;
                case 7: rSize = v; break;
                case 8: rid = (uint8_t)v; break;
                case 9: rCount = v; break;
            }
        } else if (type == 2) {
            if (tag == 0) { if (nUsages < 24) usages[nUsages++] = v & 0xFFFF; }
            else if (tag == 1) { uMin = v & 0xFFFF; haveMin = true; }
        } else if (type == 0) {
            if (tag == 8) {  /* Input */
                mouse_map_t *m = &byId[rid & 15]; m->id = rid;
                uint16_t *off = &offs[rid & 15];
                if (!(v & 1)) {
                    for (uint32_t n = 0; n < rCount; n++) {
                        uint32_t u = nUsages ? (n < nUsages ? usages[n] : usages[nUsages - 1])
                                             : (haveMin ? uMin + n : 0);
                        uint16_t bit = *off + (uint16_t)(n * rSize);
                        if (usagePage == 9) {
                            if (m->btnOff < 0) m->btnOff = bit;
                            if (m->btnCnt < 8) m->btnCnt++;
                        } else if (usagePage == 1 && rSize <= 16) {
                            if (u == 0x30)      { m->xOff = bit; m->xSz = (uint8_t)rSize; }
                            else if (u == 0x31) { m->yOff = bit; m->ySz = (uint8_t)rSize; }
                            else if (u == 0x38) { m->wOff = bit; m->wSz = (uint8_t)rSize; }
                        }
                    }
                }
                *off += (uint16_t)(rSize * rCount);
            }
            nUsages = 0; haveMin = false;
        }
    }
    for (int k = 0; k < 16; k++) {
        if (byId[k].xOff >= 0 && byId[k].yOff >= 0) {
            *out = byId[k]; out->valid = true; return true;
        }
    }
    return false;
}

static int32_t get_bits(const uint8_t *r, uint16_t len, uint16_t bitOff, uint8_t size, bool sgn)
{
    uint32_t v = 0;
    for (uint8_t k = 0; k < size; k++) {
        uint16_t bi = bitOff + k;
        if ((bi >> 3) >= len) return 0;
        if (r[bi >> 3] & (1 << (bi & 7))) v |= 1u << k;
    }
    if (sgn && size < 32 && (v & (1u << (size - 1)))) v |= ~0u << size;
    return (int32_t)v;
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const *desc_report, uint16_t desc_len)
{
    g_host_mounted = 1;
    uint8_t proto = tuh_hid_interface_protocol(dev_addr, instance);
    mouse_map_t *m = (dev_addr < MAXD && instance < MAXD) ? &maps[dev_addr][instance] : NULL;
    bool ok = false;
    if (m) { map_clear(m); if (desc_report && desc_len) ok = parse_mouse(desc_report, desc_len, m); }

    {
        uint16_t vid = 0, pid = 0; tuh_vid_pid_get(dev_addr, &vid, &pid);
        dbg_printf("MOUNT dev=%u inst=%u vid:pid=%04X:%04X proto=%u descLen=%u parsed=%d id=%u x=%d/%u y=%d/%u\r\n",
                   dev_addr, instance, vid, pid, proto, desc_len, ok,
                   m ? m->id : 0, m ? m->xOff : -9, m ? m->xSz : 0, m ? m->yOff : -9, m ? m->ySz : 0);
        dbg_printf("DESC:");
        for (uint16_t i = 0; i < desc_len && i < 96; i++) dbg_printf(" %02X", desc_report[i]);
        dbg_printf("\r\n");
    }

    if (proto == HID_ITF_PROTOCOL_MOUSE) {
        /* Standard boot mouse: fixed layout [btn][x][y][wheel], no parsing needed */
        if (dev_addr < MAXD && instance < MAXD) boot_mouse[dev_addr][instance] = true;
        tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_BOOT);
    } else if (ok) {
        if (tuh_hid_get_protocol(dev_addr, instance) != HID_PROTOCOL_REPORT)
            tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_REPORT);
    }
    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    if (dev_addr < MAXD && instance < MAXD) { map_clear(&maps[dev_addr][instance]); boot_mouse[dev_addr][instance] = false; }
    g_host_mounted = 0;
    hid_device_send_mouse(0, 0, 0, 0);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                uint8_t const *report, uint16_t len)
{
    if (len == 0) { tuh_hid_receive_report(dev_addr, instance); return; }
    g_host_reports++;
    if (g_host_reports <= 60 || (g_host_reports % 200) == 0) {
        dbg_printf("RPT #%lu dev=%u inst=%u len=%u:", (unsigned long)g_host_reports, dev_addr, instance, len);
        for (uint16_t i = 0; i < len && i < 16; i++) dbg_printf(" %02X", report[i]);
        dbg_printf("\r\n");
    }
    mouse_map_t *m = (dev_addr < MAXD && instance < MAXD) ? &maps[dev_addr][instance] : NULL;
    bool boot = (dev_addr < MAXD && instance < MAXD) && boot_mouse[dev_addr][instance];

    if (boot) {
        if (len >= 3 && (report[1] || report[2])) g_host_motion++;
        if (len >= 3)
            hid_device_send_mouse(report[0] & 0x1F, (int8_t)report[1], (int8_t)report[2],
                                  len >= 4 ? (int8_t)report[3] : 0);
    } else if (m && m->valid) {
        const uint8_t *r = report; uint16_t l = len; bool match = true;
        if (m->id != 0) {
            if (len < 1 || report[0] != m->id) match = false;
            else { r = report + 1; l = len - 1; }
        }
        if (match) {
            uint8_t btn = 0;
            if (m->btnOff >= 0) btn = (uint8_t)get_bits(r, l, m->btnOff, m->btnCnt, false) & 0x1F;
            int32_t x = get_bits(r, l, m->xOff, m->xSz, true);
            int32_t y = get_bits(r, l, m->yOff, m->ySz, true);
            int32_t w = (m->wOff >= 0) ? get_bits(r, l, m->wOff, m->wSz, true) : 0;
            if (x || y || w) g_host_motion++;
            hid_device_send_mouse(btn, x, y, w);
        }
    }
    tuh_hid_receive_report(dev_addr, instance);
}
