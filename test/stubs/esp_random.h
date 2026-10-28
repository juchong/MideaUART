#pragma once

#include <cstdint>

inline uint32_t esp_random() {
  static uint32_t state = 0x1A2B3C4D;
  state = state * 1664525U + 1013904223U;
  return state;
}
