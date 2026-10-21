#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Translate a Windows Virtual-Key code into:
 *   hid_kc  - USB HID keycode  (0 for pure modifier keys)
 *   mod     - modifier-byte mask bits (0 if not a modifier)
 */
void vk_to_hid(uint8_t vk, uint8_t *hid_kc, uint8_t *mod);
