#include "Appliance/AirConditioner/GroupData.h"
#include <limits>

namespace dudanov {
namespace midea {
namespace ac {

static uint8_t minimumSize(GroupId group) {
  switch (group) {
    case GroupId::RUNTIME:
      return 20;
    case GroupId::PERFORMANCE:
      return 16;
    case GroupId::INDOOR_FAN:
      return 13;
    case GroupId::OUTDOOR_FAN:
      return 12;
    case GroupId::ENERGY:
      return 20;
    case GroupId::ENVIRONMENT:
      return 12;
    case GroupId::OUTDOOR_POWER:
      return 13;
    case GroupId::LOUVERS:
      return 14;
  }
  return 0xFF;
}

static uint16_t readU16BE(const uint8_t *data) {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
}

static uint16_t readU16LE(const uint8_t *data) {
  return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
}

static float hours(const uint8_t *data) {
  const uint32_t seconds =
      (static_cast<uint32_t>(readU16BE(data)) * 24UL * 60UL * 60UL) +
      (static_cast<uint32_t>(data[2]) * 60UL * 60UL) +
      (static_cast<uint32_t>(data[3]) * 60UL) + data[4];
  return static_cast<float>(seconds) / 3600.0F;
}

static uint8_t bcd(uint8_t value) {
  return static_cast<uint8_t>(((value >> 4) * 10) + (value & 0x0F));
}

static uint32_t parseValue(const uint8_t *data, uint8_t size, PowerFormat format) {
  uint32_t value = 0;
  const uint8_t method = static_cast<uint8_t>(format) % 10;
  for (uint8_t index = 0; index < size; ++index) {
    if (method == static_cast<uint8_t>(PowerFormat::BCD))
      value = static_cast<uint32_t>(bcd(data[index])) + value * 100UL;
    else if (method == static_cast<uint8_t>(PowerFormat::BINARY))
      value = static_cast<uint32_t>(data[index]) + (value << 8);
    else
      value = static_cast<uint32_t>(data[index]) + value * 100UL;
  }
  return value;
}

static float parseEnergy(const uint8_t *data, PowerFormat format) {
  if (format == PowerFormat::BCD_ENERGY_BINARY_POWER)
    return static_cast<float>(parseValue(data, 4, PowerFormat::BCD)) / 100.0F;
  const float divisor = format == PowerFormat::BINARY ? 10.0F : 100.0F;
  return static_cast<float>(parseValue(data, 4, format)) / divisor;
}

static float parsePower(const uint8_t *data, PowerFormat format) {
  if (format == PowerFormat::BCD_ENERGY_BINARY_POWER)
    format = PowerFormat::BINARY;
  return static_cast<float>(parseValue(data, 3, format)) / 10.0F;
}

static float parseTemperature(uint8_t raw, float offset) {
  return raw == 0xFF ? std::numeric_limits<float>::quiet_NaN()
                     : (static_cast<float>(raw) - offset) / 2.0F;
}

bool decodeGroupData(const FrameData &frame, GroupId expected_group, PowerFormat power_format, GroupData &result) {
  const uint8_t group = static_cast<uint8_t>(expected_group);
  if (frame.size() < minimumSize(expected_group) || !frame.hasValidIntegrity() ||
      frame.at(0) != 0xC1 || frame.at(1) != 0x21 || frame.at(2) != 0x01 ||
      (frame.at(3) & 0x0F) != group)
    return false;

  result = GroupData{};
  result.group = expected_group;
  const uint8_t *data = frame.data();
  switch (expected_group) {
    case GroupId::RUNTIME:
      result.runtime.electrified_hours = hours(data + 4);
      result.runtime.total_operating_hours = hours(data + 9);
      result.runtime.current_operating_hours = hours(data + 14);
      return true;
    case GroupId::PERFORMANCE:
      result.performance.compressor_frequency = data[4];
      result.performance.target_compressor_frequency = data[5];
      result.performance.compressor_current = data[7];
      result.performance.compressor_voltage = data[8];
      result.performance.indoor_ambient_temperature = parseTemperature(data[10], 30.0F);
      result.performance.indoor_coil_temperature = parseTemperature(data[11], 30.0F);
      result.performance.outdoor_coil_temperature = parseTemperature(data[12], 50.0F);
      result.performance.outdoor_ambient_temperature = parseTemperature(data[13], 50.0F);
      result.performance.discharge_pipe_temperature = data[14];
      return true;
    case GroupId::INDOOR_FAN:
      result.indoor_fan.target_speed = static_cast<uint16_t>(data[4]) * 8U;
      result.indoor_fan.actual_speed = static_cast<uint16_t>(data[5]) * 8U;
      result.indoor_fan.water_pump_running = (data[8] & 0x10) != 0;
      result.indoor_fan.fault_flags = {data[6], data[7], data[8]};
      result.indoor_fan.protection_flags = {data[9], data[10], data[11]};
      return true;
    case GroupId::OUTDOOR_FAN:
      result.outdoor_fan.actual_speed = static_cast<uint16_t>(data[10]) * 8U;
      return true;
    case GroupId::ENERGY:
      result.energy.total_energy_kwh = parseEnergy(data + 4, power_format);
      result.energy.total_operating_energy_kwh = parseEnergy(data + 8, power_format);
      result.energy.current_energy_kwh = parseEnergy(data + 12, power_format);
      result.energy.realtime_power_w = parsePower(data + 16, power_format);
      return true;
    case GroupId::ENVIRONMENT:
      result.environment.indoor_humidity = data[4];
      result.environment.outdoor_fan_speed = static_cast<uint16_t>(data[8]) * 8U;
      result.environment.defrosting = data[10] != 0;
      return true;
    case GroupId::OUTDOOR_POWER:
      result.outdoor_power.power_w = readU16LE(data + 10);
      return true;
    case GroupId::LOUVERS:
      result.louvers.horizontal_angle = data[9];
      result.louvers.vertical_angle = data[12];
      return true;
  }
  return false;
}

}  // namespace ac
}  // namespace midea
}  // namespace dudanov
