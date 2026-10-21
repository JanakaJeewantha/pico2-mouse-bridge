#include "dbg.h"
#include "tusb.h"
#include "hardware/sync.h"
#include <stdarg.h>
#include <stdio.h>

#define RING 8192
static char ring[RING];
static volatile uint32_t head = 0, tail = 0;

void dbg_printf(const char *fmt, ...)
{
    char tmp[200];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if (n > (int)sizeof tmp - 1) n = sizeof tmp - 1;
    spin_lock_t *sl = spin_lock_instance(PICO_SPINLOCK_ID_OS1);
    uint32_t sv = spin_lock_blocking(sl);
    uint32_t h = head;
    for (int i = 0; i < n; i++) {
        if (((h + 1) % RING) == tail) break;   /* full: drop */
        ring[h] = tmp[i]; h = (h + 1) % RING;
    }
    head = h;
    spin_unlock(sl, sv);
}

void dbg_task(void)
{
    if (!tud_cdc_connected()) { tail = head; return; }
    while (tail != head && tud_cdc_write_available() > 0) {
        tud_cdc_write_char(ring[tail]);
        tail = (tail + 1) % RING;
    }
    tud_cdc_write_flush();
}
