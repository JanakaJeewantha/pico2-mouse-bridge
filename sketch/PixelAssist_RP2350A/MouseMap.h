#pragma once
#include <stdint.h>

struct MouseMap {
  bool    valid  = false;
  uint8_t id     = 0;
  int16_t btnOff = -1; uint8_t btnCnt = 0;
  int16_t xOff = -1, yOff = -1, wOff = -1;
  uint8_t xSz = 0, ySz = 0, wSz = 0;
};
static MouseMap maps[8][8];
