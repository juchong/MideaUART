#pragma once
#include "Helpers/Platform.h"
#include "Appliance/ApplianceBase.h"
#include "Appliance/AirConditioner/Capabilities.h"
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
};

class AirConditioner : public ApplianceBase {
 public:
  AirConditioner() : ApplianceBase(AIR_CONDITIONER) {}
  void m_setup() override;
  void m_onIdle() override { this->m_getStatus(); }
  void control(const Control &control);
  void setPowerState(bool state);
  bool getPowerState() const { return this->m_mode != Mode::MODE_OFF; }
  void togglePowerState() { this->setPowerState(this->m_mode == Mode::MODE_OFF); }
  float getTargetTemp() const { return this->m_targetTemp; }
  float getIndoorTemp() const { return this->m_indoorTemp; }
  float getOutdoorTemp() const { return this->m_outdoorTemp; }
  float getIndoorHum() const { return this->m_indoorHumidity; }
  float getPowerUsage() const { return this->m_powerUsage; }
  Mode getMode() const { return this->m_mode; }
  SwingMode getSwingMode() const { return this->m_swingMode; }
  FanMode getFanMode() const { return this->m_fanMode; }
  Preset getPreset() const { return this->m_preset; }
  const Capabilities &getCapabilities() const { return this->m_capabilities; }
  void displayToggle() { this->m_displayToggle(); }
  /// Ask for power usage (group 4) every 30 s. On by default; units that never answer
  /// it hold the bus for every retry, so turn it off when nobody reads it.
  void setPowerQuery(bool state) { this->m_powerQuery = state; }
  /// Fault code from the status frame (0 = none)
  uint8_t getErrorCode() const { return this->m_errorCode; }
  /// Raw appliance-error flag from status byte 1, independent of the numeric fault code.
  bool getApplianceError() const { return this->m_applianceError; }
  /// The unit's own filter cleaning reminder
  bool getFilterAlert() const { return this->m_filterAlert; }
  /// Queue an engineering group data query. `onData` receives the 0xC1 reply body
  /// (byte 0 = 0xC1) for this group; `onError` runs when all attempts fail.
  void queryGroup(uint8_t group, std::function<void(const FrameData &)> onData, Handler onError = nullptr);
 protected:
  void m_getPowerUsage();
  void m_getCapabilities();
  void m_getStatus();
  void m_setStatus(StatusData status);
  void m_flushPendingControl();
  void m_displayToggle();
  ResponseStatus m_readStatus(FrameData data);
  Capabilities m_capabilities{};
  Timer m_powerUsageTimer;
  float m_indoorHumidity{};
  float m_indoorTemp{};
  float m_outdoorTemp{};
  float m_targetTemp{};
  float m_powerUsage{};
  Mode m_mode{Mode::MODE_OFF};
  Preset m_preset{Preset::PRESET_NONE};
  FanMode m_fanMode{FanMode::FAN_AUTO};
  SwingMode m_swingMode{SwingMode::SWING_OFF};
  Preset m_lastPreset{Preset::PRESET_NONE};
  StatusData m_status{};
  bool m_sendControl{};
  bool m_powerQuery{true};
  bool m_applianceError{};
  uint8_t m_errorCode{};
  bool m_filterAlert{};
  Control m_pendingControl{};
};

}  // namespace ac
}  // namespace midea
}  // namespace dudanov
