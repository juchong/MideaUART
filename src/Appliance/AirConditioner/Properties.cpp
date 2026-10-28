#include "Appliance/AirConditioner/Properties.h"

namespace dudanov {
namespace midea {
namespace ac {

PropertyWrite PropertyWrite::boolean(PropertyId id, bool state) {
  return PropertyWrite{static_cast<uint16_t>(id), {static_cast<uint8_t>(state ? 1 : 0)}};
}

PropertyWrite PropertyWrite::scalar(PropertyId id, uint8_t value) {
  return PropertyWrite{static_cast<uint16_t>(id), {value}};
}

PropertyWrite PropertyWrite::breezeAway(bool state) {
  return PropertyWrite{static_cast<uint16_t>(PropertyId::BREEZE_AWAY),
                       {static_cast<uint8_t>(state ? 2 : 1)}};
}

PropertyWrite PropertyWrite::freshAir(bool state, uint8_t fan_speed) {
  return PropertyWrite{static_cast<uint16_t>(PropertyId::FRESH_AIR),
                       {static_cast<uint8_t>(state ? 1 : 0), fan_speed, 0xFF}};
}

PropertyWrite PropertyWrite::ieco(bool state, uint8_t level_count) {
  std::vector<uint8_t> value{0x00, level_count, static_cast<uint8_t>(state ? 1 : 0)};
  value.resize(13, 0x00);
  return PropertyWrite{static_cast<uint16_t>(PropertyId::IECO), std::move(value)};
}

PropertyWrite PropertyWrite::outdoorSilent(bool state) {
  return PropertyWrite{static_cast<uint16_t>(PropertyId::OUTDOOR_SILENT),
                       {static_cast<uint8_t>(state ? 3 : 0)}};
}

PropertyWrite PropertyWrite::cascade(bool state, uint8_t direction) {
  return PropertyWrite{static_cast<uint16_t>(PropertyId::CASCADE),
                       {static_cast<uint8_t>(state ? 1 : 0), direction}};
}

QueryPropertiesData::QueryPropertiesData(const std::vector<uint16_t> &ids) : FrameData(2 + ids.size() * 2) {
  this->m_data[0] = 0xB1;
  this->m_data[1] = static_cast<uint8_t>(ids.size());
  uint8_t offset = 2;
  for (uint16_t id : ids) {
    this->m_data[offset++] = static_cast<uint8_t>(id & 0xFF);
    this->m_data[offset++] = static_cast<uint8_t>(id >> 8);
  }
  this->m_data.push_back(FrameData::m_getID());
  this->appendCRC();
}

SetPropertiesData::SetPropertiesData(const std::vector<PropertyWrite> &properties) : FrameData(2) {
  this->m_data[0] = 0xB0;
  this->m_data[1] = static_cast<uint8_t>(properties.size());
  for (const auto &property : properties) {
    this->m_data.push_back(static_cast<uint8_t>(property.id & 0xFF));
    this->m_data.push_back(static_cast<uint8_t>(property.id >> 8));
    this->m_data.push_back(static_cast<uint8_t>(property.data.size()));
    this->m_data.insert(this->m_data.end(), property.data.begin(), property.data.end());
  }
  this->m_data.push_back(FrameData::m_getID());
  this->appendCRC();
}

bool decodeProperties(const FrameData &frame, std::vector<PropertyValue> &properties) {
  if (frame.size() < 4 || (frame.at(0) != 0xB0 && frame.at(0) != 0xB1) || !frame.hasValidIntegrity())
    return false;

  properties.clear();
  const uint8_t count = frame.at(1);
  uint8_t offset = 2;
  for (uint8_t index = 0; index < count; ++index) {
    if (static_cast<uint16_t>(offset) + 4U > frame.size() - 1U)
      return false;
    PropertyValue property;
    property.id = static_cast<uint16_t>(frame.at(offset) | (static_cast<uint16_t>(frame.at(offset + 1)) << 8));
    property.status = frame.at(offset + 2);
    const uint8_t length = frame.at(offset + 3);
    offset += 4;
    if (static_cast<uint16_t>(offset) + length > frame.size() - 1U)
      return false;
    property.data.assign(frame.data() + offset, frame.data() + offset + length);
    offset += length;
    properties.push_back(std::move(property));
  }
  return true;
}

}  // namespace ac
}  // namespace midea
}  // namespace dudanov
