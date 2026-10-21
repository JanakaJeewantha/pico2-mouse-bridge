#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Shared mouse state (written by host task on core0, read by device task on core1) */
typedef struct {
    uint8_t buttons;
    int8_t  x;
    int8_t  y;
    int8_t  wheel;
} mouse_report_t;

extern volatile mouse_report_t g_mouse;
extern volatile bool           g_mouse_dirty;

void hid_device_task(void);
void hid_device_send_mouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel);
void hid_device_key_event(uint8_t hid_keycode, uint8_t modifier_mask, bool down);
void hid_device_send_aim(int16_t dx, int16_t dy);
