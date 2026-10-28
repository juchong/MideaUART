#pragma once

#include <cstdint>

inline int64_t &native_test_time_us() {
  static int64_t value = 0;
  return value;
}

inline int64_t esp_timer_get_time() { return native_test_time_us(); }
