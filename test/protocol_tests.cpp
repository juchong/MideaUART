#include <cassert>
#include <cmath>
#include <cstdio>
#include <deque>
#include <sstream>
#include <vector>

#include "Appliance/AirConditioner/AirConditioner.h"
#include "Appliance/AirConditioner/Capabilities.h"
#include "Appliance/AirConditioner/GroupData.h"
#include "Appliance/AirConditioner/Properties.h"
#include "fixtures/status_validation.h"

using namespace dudanov::midea;
using namespace dudanov::midea::ac;

unsigned long test_millis = 0;

static std::vector<uint8_t> bytes(const char *hex) {
  std::istringstream input(hex);
  unsigned int value;
  std::vector<uint8_t> result;
  while (input >> std::hex >> value)
    result.push_back(static_cast<uint8_t>(value));
  return result;
}

static FrameData payload(const std::vector<uint8_t> &frame) {
  assert(frame.size() >= 12);
  return FrameData(frame.data() + 10, static_cast<uint8_t>(frame.size() - 11));
}

static FrameData withCrc(const std::vector<uint8_t> &data) {
  FrameData frame(data.begin(), data.end());
  frame.appendCRC();
  return frame;
}

static bool near(float lhs, float rhs, float epsilon = 0.001F) {
  return std::fabs(lhs - rhs) < epsilon;
}

class TestAirConditioner : public AirConditioner {
 public:
  void m_setup() override {}
  void m_onIdle() override {}
  ResponseStatus parse(FrameData data) { return this->m_readStatus(std::move(data)); }
  std::vector<uint8_t> cached() const {
    return {this->m_status.data(), this->m_status.data() + this->m_status.size()};
  }
  bool controlPending() const { return this->m_sendControl; }
  void applyProperties(const std::vector<PropertyValue> &properties) {
    this->m_applyProperties(properties);
  }
};

class FakeStream : public Stream {
 public:
  std::deque<uint8_t> rx{};
  std::vector<std::vector<uint8_t>> tx{};

  int available() override { return static_cast<int>(this->rx.size()); }
  int read() override {
    const uint8_t value = this->rx.front();
    this->rx.pop_front();
    return value;
  }
  int peek() override { return this->rx.front(); }
  size_t write(uint8_t value) override { return this->write(&value, 1); }
  size_t write(const uint8_t *data, size_t size) override {
    this->tx.emplace_back(data, data + size);
    return size;
  }
  void flush() override {}
  void inject(const std::vector<uint8_t> &frame) {
    this->rx.insert(this->rx.end(), frame.begin(), frame.end());
  }
};

static void testStatusValidation() {
  TestAirConditioner device;
  int publications = 0;
  device.addOnStateCallback([&publications]() { ++publications; });

  const auto seed = bytes(STATUS_FIXTURES[0].hex);
  assert(device.parse(payload(seed)) == RESPONSE_OK);
  assert(device.getState().valid);
  assert(device.getMode() == MODE_COOL);
  assert(near(device.getTargetTemp(), 23.5F));
  const auto cached = device.cached();
  const auto revision = device.getState().revision;
  const auto accepted = device.getDiagnostics().valid_status_frames;
  const auto published = publications;

  for (const auto &fixture : STATUS_FIXTURES) {
    const auto frame = bytes(fixture.hex);
    assert(frame.size() == static_cast<size_t>(frame[1]) + 1U);
    uint8_t outer_checksum = 0;
    for (size_t index = 1; index < frame.size(); ++index)
      outer_checksum += frame[index];
    assert(outer_checksum == 0);
    const auto before_cache = device.cached();
    const auto before_revision = device.getState().revision;
    const auto result = device.parse(payload(frame));
    assert((result == RESPONSE_OK) == fixture.valid);
    if (!fixture.valid) {
      assert(device.cached() == before_cache);
      assert(device.getState().revision == before_revision);
    }
  }

  assert(device.getDiagnostics().valid_status_frames == accepted + 10);
  assert(device.getState().revision == revision + 10);
  assert(device.cached() != cached || publications >= published);

  for (uint8_t size = 1; size < 17; ++size) {
    std::vector<uint8_t> short_data(size - 1, 0);
    if (!short_data.empty())
      short_data[0] = 0xC0;
    assert(device.parse(withCrc(short_data)) == RESPONSE_WRONG);
  }

  const auto legacy = bytes("C0 00 4A 66 7F 7F 00 00 00 00 00 6B 67 00 00 00 00 00 00 C0");
  FrameData legacy_frame(legacy.begin(), legacy.end());
  assert(!legacy_frame.hasValidCRC());
  assert(legacy_frame.hasValidChecksum());
  assert(device.parse(legacy_frame) == RESPONSE_OK);
  assert(device.getMode() == MODE_OFF);
  assert(near(device.getTargetTemp(), 26.0F));
  assert(std::isnan(device.getIndoorHum()));
}

static void testExtendedC0State() {
  TestAirConditioner device;
  auto frame = withCrc({
      0xC0, 0x81, 0x4A, 0x66, 0x7F, 0x7F, 0x00, 0x00,
      0xC0, 0x3E, 0x25, 0x66, 0x6E, 0x60, 0x19, 0x00,
      0x2A, 0x00, 0x00, 55, 0x00, 0x80, 0x01, 0x00,
      0x02, 0x58, 0x00, 0x2A,
  });
  assert(device.parse(frame) == RESPONSE_OK);
  const auto &state = device.getState();
  assert(state.appliance_error.hasValue() && state.appliance_error.value());
  assert(state.c0_byte_8_bit_7.hasValue() && state.c0_byte_8_bit_7.value());
  assert(state.smart_eye.hasValue() && state.smart_eye.value());
  assert(state.natural_wind.hasValue() && state.natural_wind.value());
  assert(state.c0_byte_9_bit_2.hasValue() && state.c0_byte_9_bit_2.value());
  assert(state.c0_display_field.hasValue() && state.c0_display_field.value() == 1);
  assert(state.eco_mode.hasValue() && state.eco_mode.value());
  assert(state.sleep_mode.hasValue() && state.sleep_mode.value());
  assert(state.prevent_cold.hasValue() && state.prevent_cold.value());
  assert(state.frost_protection.hasValue() && state.frost_protection.value());
  assert(state.comfort_mode.hasValue() && state.comfort_mode.value());
  assert(state.fresh_filter_timeout.hasValue() && state.fresh_filter_timeout.value());
  assert(state.fresh_filter_total.hasValue() && state.fresh_filter_total.value() == 600);
  assert(state.fresh_filter_used.hasValue() && state.fresh_filter_used.value() == 42);
  assert(state.pmv.hasValue() && near(state.pmv.value(), 1.0F));
}

static std::vector<uint8_t> energyData(const std::vector<uint8_t> &total,
                                       const std::vector<uint8_t> &operating,
                                       const std::vector<uint8_t> &current,
                                       const std::vector<uint8_t> &power) {
  std::vector<uint8_t> data{0xC1, 0x21, 0x01, 0x44};
  data.insert(data.end(), total.begin(), total.end());
  data.insert(data.end(), operating.begin(), operating.end());
  data.insert(data.end(), current.begin(), current.end());
  data.insert(data.end(), power.begin(), power.end());
  return data;
}

static void testGroupDecoding() {
  GroupData decoded;
  auto bcd_frame = withCrc(energyData({0x00, 0x00, 0x12, 0x34}, {0, 0, 0x05, 0x67},
                                      {0, 0, 0, 0x89}, {0, 0x12, 0x34}));
  assert(decodeGroupData(bcd_frame, GroupId::ENERGY, PowerFormat::BCD, decoded));
  assert(near(decoded.energy.total_energy_kwh, 12.34F));
  assert(near(decoded.energy.total_operating_energy_kwh, 5.67F));
  assert(near(decoded.energy.current_energy_kwh, 0.89F));
  assert(near(decoded.energy.realtime_power_w, 123.4F));

  auto binary_frame = withCrc(energyData({0, 0, 0, 123}, {0, 0, 0, 45},
                                         {0, 0, 0, 7}, {0, 4, 210}));
  assert(decodeGroupData(binary_frame, GroupId::ENERGY, PowerFormat::BINARY, decoded));
  assert(near(decoded.energy.total_energy_kwh, 12.3F));
  assert(near(decoded.energy.realtime_power_w, 123.4F));

  auto binary_01_frame = withCrc(energyData({0, 0, 4, 210}, {0, 0, 0, 45},
                                            {0, 0, 0, 7}, {0, 4, 210}));
  assert(decodeGroupData(binary_01_frame, GroupId::ENERGY,
                         PowerFormat::BINARY_01_KWH, decoded));
  assert(near(decoded.energy.total_energy_kwh, 12.34F));

  assert(decodeGroupData(bcd_frame, GroupId::ENERGY,
                         PowerFormat::BCD_ENERGY_BINARY_POWER, decoded));
  assert(near(decoded.energy.total_energy_kwh, 12.34F));
  assert(near(decoded.energy.realtime_power_w, 466.0F));

  auto mixed_frame = withCrc(energyData({0, 0, 12, 34}, {0, 0, 5, 67},
                                        {0, 0, 0, 89}, {0, 12, 34}));
  assert(decodeGroupData(mixed_frame, GroupId::ENERGY, PowerFormat::MIXED, decoded));
  assert(near(decoded.energy.total_energy_kwh, 12.34F));
  assert(near(decoded.energy.realtime_power_w, 123.4F));

  auto performance = withCrc({0xC1, 0x21, 0x01, 0x41, 55, 60, 0, 9, 230,
                              0, 74, 70, 90, 86, 101});
  assert(decodeGroupData(performance, GroupId::PERFORMANCE, PowerFormat::BCD, decoded));
  assert(decoded.performance.compressor_frequency == 55);
  assert(near(decoded.performance.indoor_ambient_temperature, 22.0F));
  assert(near(decoded.performance.outdoor_ambient_temperature, 18.0F));

  auto indoor_fan = withCrc({0xC1, 0x21, 0x01, 0x42, 50, 45,
                             0x01, 0x02, 0x04, 0x08, 0x10, 0x20});
  assert(decodeGroupData(indoor_fan, GroupId::INDOOR_FAN, PowerFormat::BCD, decoded));
  assert(decoded.indoor_fan.target_speed == 400);
  assert(decoded.indoor_fan.actual_speed == 360);
  assert(!decoded.indoor_fan.water_pump_running);
  assert(decoded.raw_payload.size() == 8);
  assert(decoded.raw_payload[4] == 0x04 && decoded.raw_payload[7] == 0x20);

  auto indoor_fan_pump = withCrc({0xC1, 0x21, 0x01, 0x42, 50, 45,
                                  0x01, 0x02, 0x14, 0x08, 0x10, 0x20});
  assert(decodeGroupData(indoor_fan_pump, GroupId::INDOOR_FAN,
                         PowerFormat::BCD, decoded));
  assert(decoded.indoor_fan.water_pump_running);

  auto runtime = withCrc({0xC1, 0x21, 0x01, 0x40,
                          0, 1, 2, 30, 0,
                          0, 2, 0, 0, 0,
                          0, 0, 1, 15, 0});
  assert(decodeGroupData(runtime, GroupId::RUNTIME, PowerFormat::BCD, decoded));
  assert(decoded.raw_payload.size() == 15);
  assert(decoded.raw_payload[0] == 0x00 && decoded.raw_payload[1] == 0x01);
  assert(decoded.raw_payload[12] == 0x01 && decoded.raw_payload[13] == 0x0F);

  auto outdoor_fan = withCrc({0xC1, 0x21, 0x01, 0x43, 0, 0, 0, 0, 0, 0, 75});
  assert(decodeGroupData(outdoor_fan, GroupId::OUTDOOR_FAN, PowerFormat::BCD, decoded));
  assert(decoded.outdoor_fan.actual_speed == 600);

  auto environment = withCrc({0xC1, 0x21, 0x01, 0x45, 48, 0, 0, 0, 60, 0, 1});
  assert(decodeGroupData(environment, GroupId::ENVIRONMENT, PowerFormat::BCD, decoded));
  assert(decoded.environment.indoor_humidity == 48);
  assert(decoded.raw_payload.size() == 7);
  assert(decoded.raw_payload[4] == 60 && decoded.raw_payload[6] == 1);

  auto outdoor_power = withCrc({0xC1, 0x21, 0x01, 0x47, 0, 0, 0, 0, 0, 0, 0xD2, 0x04});
  assert(decodeGroupData(outdoor_power, GroupId::OUTDOOR_POWER, PowerFormat::BCD, decoded));
  assert(decoded.outdoor_power.power_w == 1234);

  auto louvers = withCrc({0xC1, 0x21, 0x01, 0x4B, 0, 0, 0, 0, 0, 37, 0, 0, 82});
  assert(decodeGroupData(louvers, GroupId::LOUVERS, PowerFormat::BCD, decoded));
  assert(decoded.raw_payload.size() == 9);
  assert(decoded.raw_payload[5] == 37 && decoded.raw_payload[8] == 82);

  auto corrupted = bcd_frame;
  std::vector<uint8_t> bad(corrupted.data(), corrupted.data() + corrupted.size());
  bad[4] ^= 1;
  FrameData bad_frame(bad.begin(), bad.end());
  assert(!decodeGroupData(bad_frame, GroupId::ENERGY, PowerFormat::BCD, decoded));
  assert(!decodeGroupData(bcd_frame, GroupId::OUTDOOR_POWER, PowerFormat::BCD, decoded));
}

static void testProperties() {
  QueryPropertiesData query({static_cast<uint16_t>(PropertyId::INDOOR_HUMIDITY),
                             static_cast<uint16_t>(PropertyId::UNKNOWN_00E3)});
  assert(query.hasValidCRC());
  assert(query.at(0) == 0xB1 && query.at(1) == 2);
  assert(query.at(2) == 0x15 && query.at(3) == 0x00);
  assert(query.at(4) == 0xE3 && query.at(5) == 0x00);

  SetPropertiesData set({PropertyWrite::breezeAway(true),
                         PropertyWrite::freshAir(true, 60),
                         PropertyWrite::outdoorSilent(true)});
  assert(set.hasValidCRC());
  assert(set.at(0) == 0xB0 && set.at(1) == 3);
  assert(set.at(2) == 0x42 && set.at(3) == 0x00 && set.at(4) == 1 && set.at(5) == 2);

  auto response = withCrc({0xB1, 0x02,
                           0x15, 0x00, 0x00, 0x01, 55,
                           0xE3, 0x00, 0x00, 0x03, 0, 1, 1});
  std::vector<PropertyValue> properties;
  assert(decodeProperties(response, properties));
  assert(properties.size() == 2);
  assert(properties[0].id == static_cast<uint16_t>(PropertyId::INDOOR_HUMIDITY));
  assert(properties[0].reserved == 0 && properties[0].data[0] == 55);
  assert(properties[1].data.size() == 3 && properties[1].data[2] == 1);

  auto truncated = withCrc({0xB1, 0x01, 0x15, 0x00, 0x00, 0x03, 55});
  assert(!decodeProperties(truncated, properties));

  auto with_message_id = withCrc({0xB1, 0x01, 0x15, 0x00, 0x00, 0x01, 48, 0x7B});
  uint8_t message_id = 0;
  assert(decodeProperties(with_message_id, properties, &message_id));
  assert(message_id == 0x7B);

  auto trailing_garbage = withCrc({0xB1, 0x01, 0x15, 0x00, 0x00, 0x01, 48, 1, 2});
  assert(!decodeProperties(trailing_garbage, properties));

  TestAirConditioner device;
  device.applyProperties({PropertyValue{0x7777, 0x00, {1, 2, 3}},
                          PropertyValue{static_cast<uint16_t>(PropertyId::INDOOR_HUMIDITY),
                                        0x10, {47}}});
  const auto *unknown = device.getProperty(0x7777);
  assert(unknown != nullptr && unknown->data.size() == 3 && unknown->data[2] == 3);
  const auto *reserved = device.getProperty(static_cast<uint16_t>(PropertyId::INDOOR_HUMIDITY));
  assert(reserved != nullptr && reserved->reserved == 0x10 && reserved->data[0] == 47);
  assert(device.getState().indoor_humidity.hasValue() &&
         device.getState().indoor_humidity.value() == 47);
}

static void testCapabilities() {
  auto report = withCrc({0xB5, 0x03,
                         0x1E, 0x02, 0x01, 0x01,
                         0xCD, 0x00, 0x01, 0x03,
                         0x25, 0x02, 0x07, 0x20, 0x3C, 0x20, 0x3C, 0x20, 0x3C, 0x00});
  Capabilities capabilities;
  assert(!capabilities.read(report));
  assert(capabilities.valid());
  assert(capabilities.supportAnion());
  assert(capabilities.reported(0x00CD));
  assert(capabilities.get(0x00CD) != nullptr && capabilities.get(0x00CD)->data[0] == 3);
  assert(capabilities.minTempCool() == 16.0F && capabilities.maxTempHeat() == 30.0F);
  assert(capabilities.records().size() == 3);

  auto malformed = withCrc({0xB5, 0x01, 0x25, 0x02, 0x07, 0x20, 0x3C});
  Capabilities broken;
  assert(!broken.read(malformed));
  assert(!broken.valid());

  auto semantic_report = withCrc({
      0xB5, 0x09,
      0x14, 0x02, 0x01, 13,
      0x15, 0x02, 0x01, 3,
      0x10, 0x02, 0x01, 1,
      0x16, 0x02, 0x01, 5,
      0x48, 0x00, 0x01, 3,
      0x24, 0x02, 0x01, 100,
      0x1E, 0x02, 0x01, 1,
      0x1F, 0x02, 0x01, 3,
      0x2C, 0x02, 0x01, 1,
  });
  Capabilities semantic;
  assert(!semantic.read(semantic_report));
  assert(semantic.valid());
  assert(semantic.supportAutoMode() && semantic.supportCoolMode());
  assert(semantic.supportHeatMode() && semantic.supportDryMode());
  assert(!semantic.supportVerticalSwing() && semantic.supportHorizontalSwing());
  assert(semantic.supportCustomFan() && semantic.supportSilentFan());
  assert(semantic.supportLowFan() && semantic.supportMediumFan());
  assert(semantic.supportHighFan() && semantic.supportAutoFan());
  assert(semantic.supportEnergyStats() && semantic.supportEnergySetting());
  assert(!semantic.energyBcd());
  assert(semantic.supportRateSelect2Level() && semantic.supportRateSelect5Level());
  assert(semantic.supportLightControl() && semantic.supportAnion());
  assert(semantic.manualSetHumidity() && !semantic.autoSetHumidity());
  assert(semantic.supportSound());

  auto heat_only_report = withCrc({0xB5, 0x02,
                                   0x14, 0x02, 0x01, 10,
                                   0x10, 0x02, 0x01, 3});
  Capabilities heat_only;
  assert(!heat_only.read(heat_only_report));
  assert(heat_only.supportHeatMode() && !heat_only.supportCoolMode());
  assert(!heat_only.supportDryMode() && !heat_only.supportAutoMode());
  assert(!heat_only.supportSilentFan() && heat_only.supportLowFan());
  assert(!heat_only.supportMediumFan() && heat_only.supportHighFan());
  assert(!heat_only.supportAutoFan());

  auto paginated = withCrc({0xB5, 0x00, 0x01, 0x00});
  Capabilities page;
  assert(page.read(paginated));
  assert(page.valid());

  auto excess_trailer = withCrc({0xB5, 0x00, 0x01, 0x00, 0x00});
  Capabilities excess;
  assert(!excess.read(excess_trailer));
  assert(!excess.valid());
}

static void testConfirmedStateControl() {
  FakeStream stream;
  TestAirConditioner device;
  device.setStream(&stream);
  device.setPeriod(1);
  device.setTimeout(100);
  device.setup();
  device.loop();
  ++test_millis;
  device.loop();
  stream.tx.clear();

  Control first;
  first.targetTemp = 24.0F;
  device.control(first);
  Control second;
  second.targetTemp = 25.0F;
  device.control(second);
  device.loop();
  assert(stream.tx.size() == 1);
  assert(stream.tx.back()[9] == DEVICE_QUERY);
  assert(payload(stream.tx.back()).at(0) == 0x41);

  stream.inject(bytes(STATUS_FIXTURES[0].hex));
  ++test_millis;
  device.loop();
  assert(device.getState().valid);
  assert(stream.tx.size() == 2);
  assert(stream.tx.back()[9] == DEVICE_CONTROL);
  StatusData command{payload(stream.tx.back())};
  assert(near(command.getTargetTemp(), 25.0F));

  // A validated cached status remains a usable full-state baseline. There is
  // no invented age threshold that turns a control call into a delayed query.
  stream.inject(bytes(STATUS_FIXTURES[0].hex));
  ++test_millis;
  device.loop();
  stream.tx.clear();
  test_millis += 60000;
  Control later;
  later.targetTemp = 24.0F;
  device.control(later);
  device.loop();
  assert(stream.tx.size() == 1);
  assert(stream.tx.back()[9] == DEVICE_CONTROL);
}

static void testGroupQueryWireFormat() {
  QueryGroupData query{GroupId::ENERGY};
  const std::array<uint8_t, 7> captured_energy_query{0x41, 0x21, 0x01, 0x44, 0x00, 0x01, 0x09};
  assert(query.size() == captured_energy_query.size());
  for (uint8_t index = 0; index < captured_energy_query.size(); ++index)
    assert(query.at(index) == captured_energy_query[index]);
  assert(query.hasValidCRC());
}

int main() {
  testStatusValidation();
  testExtendedC0State();
  testGroupDecoding();
  testProperties();
  testCapabilities();
  testConfirmedStateControl();
  testGroupQueryWireFormat();
  std::puts("PASS: captured status, raw preservation, corroborated semantics, control, and wire formats");
}
