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
  VOICE_CONTROL = 0x0020,
  COOL_HEAT_SENSE = 0x0021,
  VOLUME_CONTROL = 0x0024,
  SECURITY = 0x0029,
  NOBODY_ENERGY_SAVE = 0x0030,
  INTELLIGENT_CONTROL = 0x0031,
  WIND_STRAIGHT = 0x0032,
  WIND_AVOID = 0x0033,
  INTELLIGENT_WIND = 0x0034,
  SELF_CLEAN = 0x0039,
  CHILD_PREVENT_COLD_WIND = 0x003A,
  FILTER_STATUS = 0x003D,
  ERROR_CODE = 0x003F,
  MODE_QUERY = 0x0041,
  BREEZE_AWAY = 0x0042,
  BREEZE_CONTROL = 0x0043,
  FACE_REGISTER = 0x0044,
  HIGH_TEMPERATURE_MONITOR = 0x0047,
  RATE_SELECT = 0x0048,
  PREVENT_SUPER_COOL = 0x0049,
  WATER_WASHING = 0x004A,
  FRESH_AIR = 0x004B,
  EXTREME_WIND = 0x004C,
  EVEN_WIND = 0x004E,
  SINGLE_TUYERE = 0x004F,
  WATER_PUMP = 0x0050,
  PARENT_CONTROL = 0x0051,
  PREVENT_STRAIGHT_WIND_LR = 0x0058,
  CASCADE = 0x0059,
  DEGERMING = 0x005A,
  LIGHT = 0x005B,
  CHILD_LOCK = 0x005C,
  SELF_REMOVE_ODOR_PHASE = 0x005D,
  HIGH_TEMP_REMOVE_ODOR = 0x005E,
  OZONE = 0x005F,
  WIND_TOP = 0x0061,
  SOFT_WARM = 0x0063,
  FLASH = 0x0067,
  REWARMING_DRY = 0x0068,
  AROM = 0x0069,
  FRESH_FILTER_RESET = 0x0085,
  DUST_FULL_TIME_RESET = 0x00B8,
  WET_FILM_RESET = 0x00CC,
  OUTDOOR_SILENT = 0x00CD,
  IECO = 0x00E3,
  PRE_COOL_HEAT = 0x0201,
  PM25 = 0x020B,
  CAPABILITY_FAN_SPEED = 0x0210,
  CAPABILITY_ECO = 0x0212,
  CAPABILITY_FROST_PROTECTION = 0x0213,
  CAPABILITY_MODES = 0x0214,
  CAPABILITY_SWING = 0x0215,
  CAPABILITY_ENERGY = 0x0216,
  CAPABILITY_FILTER_REMINDER = 0x0217,
  CAPABILITY_AUX_HEAT = 0x0219,
  CAPABILITY_TURBO = 0x021A,
  LITTLE_ANGEL = 0x021B,
  ANION = 0x021E,
  CAPABILITY_HUMIDITY = 0x021F,
  CAPABILITY_FILTER_CHECK = 0x0221,
  CAPABILITY_FAHRENHEIT = 0x0222,
  CAPABILITY_DISPLAY = 0x0224,
  CAPABILITY_TEMPERATURE = 0x0225,
  AUTO_PREVENT_STRAIGHT_WIND = 0x0226,
  REMOTE_CONTROL_LOCK = 0x0227,
  OPERATING_TIME = 0x0228,
  PTC_LOCK = 0x0229,
  OFFLINE_OPERATING_TIME = 0x022B,
  SOUND = 0x022C,
  FRESH_AIR_V1 = 0x0233,
  FRESH_AIR_PARAMETERS = 0x0250,
  FILTER_LEVEL = 0x0409,
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

bool decodeProperties(const FrameData &frame, std::vector<PropertyValue> &properties,
                      uint8_t *message_id = nullptr);

}  // namespace ac
}  // namespace midea
}  // namespace dudanov
