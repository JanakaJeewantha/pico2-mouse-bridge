#pragma once
void dbg_printf(const char *fmt, ...);   /* core1 (producer) */
void dbg_task(void);                     /* core0 (consumer)  */
