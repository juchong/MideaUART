#pragma once
#include "Helpers/Platform.h"
#include "Appliance/ApplianceBase.h"
#include "Appliance/AirConditioner/Capabilities.h"
#include "Appliance/AirConditioner/GroupData.h"
#include "Appliance/AirConditioner/Properties.h"
#include "Appliance/AirConditioner/StatusData.h"
#include "Helpers/Helpers.h"

namespace dudanov {
namespace midea {
namespace ac {

// Air conditioner control command
struct Control {
  Optional<float> targetTemp{};
  Optional<Mode> mode{};
  Optional<Preset> preset{};
  Optional<FanMode> fanMode{};
  Optional<SwingMode> swingMode{};
  Optional<bool> purifier{};
  Optional<bool> auxHeat{};
  Optional<bool> fahrenheit{};
  Optional<uint8_t> targetHumidity{};
};

struct AirConditionerState {
  bool valid{};
  uint32_t received_at{};
  uint32_t revision{};
  Optional<Mode> mode{};
  Optional<Preset> preset{};
  Optional<FanMode> fan_mode{};
  Optional<SwingMode> swing_mode{};
  Optional<float> target_temperature{};
  Optional<float> indoor_temperature{};
  Optional<float> outdoor_temperature{};
  Optional<uint8_t> target_humidity{};
  Optional<uint8_t> indoor_humidity{};
  Optional<bool> appliance_error{};
  Optional<uint8_t> error_code{};
  Optional<bool> filter_alert{};
  Optional<bool> display_on{};
  Optional<uint8_t> display_level{};
  Optional<bool> follow_me{};
  Optional<bool> purifier{};
  Optional<bool> aux_heat{};
  Optional<bool> power_saving{};
  Optional<bool> smart_eye{};
  Optional<bool> natural_wind{};
  Optional<bool> dry{};
  Optional<bool> eco_mode{};
  Optional<bool> sleep_mode{};
  Optional<bool> prevent_cold{};
  Optional<float> pmv{};
  Optional<bool> frost_protection{};
  Optional<bool> comfort_mode{};
  Optional<bool> fresh_filter_timeout{};
  Optional<uint16_t> fresh_filter_total{};
  Optional<uint16_t> fresh_filter_used{};
  Optional<bool> fahrenheit{};
  Optional<bool> defrosting{};
  Optional<float> electrified_hours{};
  Optional<float> total_operating_hours{};
  Optional<float> current_operating_hours{};
  Optional<float> realtime_power{};
  Optional<float> total_energy{};
  Optional<float> total_operating_energy{};
  Optional<float> current_energy{};
  Optional<uint16_t> outdoor_unit_power{};
  Optional<uint8_t> compressor_frequency{};
  Optional<uint8_t> target_compressor_frequency{};
  Optional<uint8_t> compressor_current{};
  Optional<uint8_t> compressor_voltage{};
  Optional<float> indoor_coil_temperature{};
  Optional<float> outdoor_coil_temperature{};
  Optional<uint8_t> discharge_pipe_temperature{};
  Optional<uint16_t> target_indoor_fan_speed{};
  Optional<uint16_t> indoor_fan_speed{};
  Optional<uint16_t> outdoor_fan_speed{};
  Optional<uint32_t> indoor_unit_fault_flags{};
  Optional<uint32_t> indoor_unit_protection_flags{};
  Optional<bool> water_pump_running{};
  Optional<uint8_t> horizontal_louver_angle{};
  Optional<uint8_t> vertical_louver_angle{};
  Optional<bool> breezeless{};
  Optional<bool> prompt_tone{};
  Optional<bool> self_clean{};
  Optional<bool> breeze_away{};
  Optional<uint8_t> breeze_control{};
  Optional<uint8_t> rate_select{};
  Optional<uint8_t> fresh_air_speed{};
  Optional<bool> cascade{};
  Optional<uint8_t> cascade_direction{};
  Optional<bool> flash{};
  Optional<bool> outdoor_silent{};
  Optional<bool> ieco{};
  Optional<bool> sound{};
};

struct CommunicationDiagnostics {
  uint32_t valid_status_frames{};
  uint32_t rejected_status_frames{};
  uint32_t group_failures{};
  uint32_t property_failures{};
};

using GroupDataCallback = std::function<void(const GroupData &)>;
using PropertiesCallback = std::function<void(const std::vector<PropertyValue> &)>;

class AirConditioner : public ApplianceBase {
 public:
  AirConditioner() : ApplianceBase(AIR_CONDITIONER) {}
  void m_setup() override;
  void m_onIdle() override { this->m_getStatus(); }
  void control(const Control &control);
  bool requestGroup(GroupId group, GroupDataCallback onData, Handler onError = nullptr, uint8_t attempts = 1);
  bool requestProperties(const std::vector<uint16_t> &ids, PropertiesCallback onData, Handler onError = nullptr);
  bool setProperties(const std::vector<PropertyWrite> &properties, PropertiesCallback onData = nullptr,
                     Handler onError = nullptr);
  void setPowerState(bool state);
  bool getPowerState() const { return this->getMode() != Mode::MODE_OFF; }
  void togglePowerState() { this->setPowerState(!this->getPowerState()); }
  float getTargetTemp() const;
  float getIndoorTemp() const;
  float getOutdoorTemp() const;
  float getIndoorHum() const;
  float getPowerUsage() const;
  Mode getMode() const;
  SwingMode getSwingMode() const;
  FanMode getFanMode() const;
  Preset getPreset() const;
  bool getApplianceError() const;
  uint8_t getErrorCode() const;
  bool getFilterAlert() const;
  bool hasFreshState(uint32_t max_age = 15000) const;
  uint32_t getStateAge() const;
  const AirConditionerState &getState() const { return this->m_state; }
  const CommunicationDiagnostics &getDiagnostics() const { return this->m_diagnostics; }
  const Capabilities &getCapabilities() const { return this->m_capabilities; }
  const std::vector<PropertyValue> &getProperties() const { return this->m_properties; }
  const PropertyValue *getProperty(uint16_t id) const;
  void setPowerQuery(bool state) { this->m_powerQuery = state; }
  void setPowerFormat(PowerFormat format) { this->m_powerFormat = format; }
  void displayToggle() { this->m_displayToggle(); }
 protected:
  void m_getPowerUsage();
  void m_getCapabilities();
  void m_getStatus();
  void m_setStatus(StatusData status);
  void m_flushPendingControl();
  void m_displayToggle();
  ResponseStatus m_readStatus(FrameData data);
  void m_applyGroup(const GroupData &data);
  void m_applyProperties(const std::vector<PropertyValue> &properties);
  Capabilities m_capabilities{};
  std::vector<PropertyValue> m_properties{};
  Timer m_powerUsageTimer;
  AirConditionerState m_state{};
  CommunicationDiagnostics m_diagnostics{};
  Preset m_lastPreset{Preset::PRESET_NONE};
  StatusData m_status{};
  Control m_pendingControl{};
  bool m_sendControl{};
  bool m_statusPending{};
  bool m_powerQuery{true};
  bool m_propertyPending{};
  bool m_groupPending[12]{};
  PowerFormat m_powerFormat{PowerFormat::BCD};
};

}  // namespace ac
}  // namespace midea
}  // namespace dudanov
