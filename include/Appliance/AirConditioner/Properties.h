#pragma once

#include <vector>

#include "Frame/FrameData.h"

namespace dudanov {
namespace midea {
namespace ac {

enum class PropertyId : uint16_t {
  SWING_VERTICAL_ANGLE = 0x0009,
  SWING_HORIZONTAL_ANGLE = 0x000A,
  INDOOR_HUMIDITY = 0x0015,
  DISPLAY_CONTROL = 0x0017,
  BREEZELESS = 0x0018,
  PROMPT_TONE = 0x001A,
  SELF_CLEAN = 0x0039,
  FILTER_STATUS = 0x003D,
  ERROR_CODE = 0x003F,
  BREEZE_AWAY = 0x0042,
  BREEZE_CONTROL = 0x0043,
  RATE_SELECT = 0x0048,
  FRESH_AIR = 0x004B,
  WATER_PUMP = 0x0050,
  CASCADE = 0x0059,
  FLASH = 0x0067,
  OUTDOOR_SILENT = 0x00CD,
  IECO = 0x00E3,
  ANION = 0x021E,
  SOUND = 0x022C,
};

struct PropertyValue {
  uint16_t id{};
  uint8_t status{};
  std::vector<uint8_t> data{};
  bool successful() const { return (this->status & 0x10) == 0; }
};

struct PropertyWrite {
  uint16_t id{};
  std::vector<uint8_t> data{};

  static PropertyWrite boolean(PropertyId id, bool state);
  static PropertyWrite scalar(PropertyId id, uint8_t value);
  static PropertyWrite breezeAway(bool state);
  static PropertyWrite freshAir(bool state, uint8_t fan_speed);
  static PropertyWrite ieco(bool state, uint8_t level_count = 1);
  static PropertyWrite outdoorSilent(bool state);
  static PropertyWrite cascade(bool state, uint8_t direction = 0);
};

class QueryPropertiesData : public FrameData {
 public:
  explicit QueryPropertiesData(const std::vector<uint16_t> &ids);
};

class SetPropertiesData : public FrameData {
 public:
  explicit SetPropertiesData(const std::vector<PropertyWrite> &properties);
};

bool decodeProperties(const FrameData &frame, std::vector<PropertyValue> &properties);

}  // namespace ac
}  // namespace midea
}  // namespace dudanov
