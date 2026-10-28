#pragma once

#include <vector>

#include "Frame/FrameData.h"

namespace dudanov {
namespace midea {
namespace ac {

enum class GroupId : uint8_t {
  RUNTIME = 0,
  PERFORMANCE = 1,
  INDOOR_FAN = 2,
  OUTDOOR_FAN = 3,
  ENERGY = 4,
  ENVIRONMENT = 5,
  OUTDOOR_POWER = 7,
  LOUVERS = 11,
};

enum class PowerFormat : uint8_t {
  BCD = 1,
  BINARY = 2,
  MIXED = 3,
  BINARY_01_KWH = 12,
  BCD_ENERGY_BINARY_POWER = 101,
};

struct PerformanceData {
  uint8_t compressor_frequency{};
  uint8_t target_compressor_frequency{};
  uint8_t compressor_current{};
  uint8_t compressor_voltage{};
  float indoor_ambient_temperature{};
  float indoor_coil_temperature{};
  float outdoor_coil_temperature{};
  float outdoor_ambient_temperature{};
  uint8_t discharge_pipe_temperature{};
};

struct IndoorFanData {
  uint16_t target_speed{};
  uint16_t actual_speed{};
  bool water_pump_running{};
};

struct OutdoorFanData {
  uint16_t actual_speed{};
};

struct EnergyData {
  float total_energy_kwh{};
  float total_operating_energy_kwh{};
  float current_energy_kwh{};
  float realtime_power_w{};
};

struct EnvironmentData {
  uint8_t indoor_humidity{};
};

struct OutdoorPowerData {
  uint16_t power_w{};
};

struct GroupData {
  GroupId group{GroupId::RUNTIME};
  /// Bytes after the four-byte C1 group header, excluding the integrity byte.
  /// Preserved for groups and fields whose semantics are not established.
  std::vector<uint8_t> raw_payload{};
  PerformanceData performance{};
  IndoorFanData indoor_fan{};
  OutdoorFanData outdoor_fan{};
  EnergyData energy{};
  EnvironmentData environment{};
  OutdoorPowerData outdoor_power{};
};

class QueryGroupData : public FrameData {
 public:
  explicit QueryGroupData(GroupId group) :
      FrameData({0x41, 0x21, 0x01, static_cast<uint8_t>(0x40 | static_cast<uint8_t>(group)),
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) {
    this->appendCRC();
  }
};

bool decodeGroupData(const FrameData &frame, GroupId expected_group, PowerFormat power_format, GroupData &result);

}  // namespace ac
}  // namespace midea
}  // namespace dudanov
