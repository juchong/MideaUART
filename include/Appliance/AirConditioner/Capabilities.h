#pragma once
#include "Helpers/Platform.h"
#include <vector>

namespace dudanov {
namespace midea {

class FrameData;

namespace ac {

class Capabilities {
 public:
  struct Record {
    uint16_t id{};
    std::vector<uint8_t> data{};
  };

  // Read from frames
  bool read(const FrameData &data);
  bool valid() const { return this->m_valid; }
  bool reported(uint16_t id) const;
  bool has(uint16_t id) const;
  const Record *get(uint16_t id) const;
  const std::vector<Record> &records() const { return this->m_records; }
  // Dump capabilities
  void dump() const;

  // Control humidity
  bool autoSetHumidity() const { return this->m_autoSetHumidity; };
  bool activeClean() const { return this->m_activeClean; };
  bool breezeControl() const { return this->m_breezeControl; };
  bool buzzer() const { return this->m_buzzer; }
  bool decimals() const { return this->m_decimals; }
  bool electricAuxHeating() const { return this->m_electricAuxHeating; }
  bool fanSpeedControl() const { return this->m_fanSpeedControl; }
  bool indoorHumidity() const { return this->m_indoorHumidity; }
  // Control humidity
  bool manualSetHumidity() const { return this->m_manualSetHumidity; }
  bool oneKeyNoWindOnMe() const { return this->m_oneKeyNoWindOnMe; }
  bool powerCal() const { return this->m_powerCal; }
  bool powerCalSetting() const { return this->m_powerCalSetting; }
  bool energyBcd() const { return this->m_energyBcd; }
  bool silkyCool() const { return this->m_silkyCool; }
  // Temperature unit can be changed between Celsius and Fahrenheit
  bool unitChangeable() const { return this->m_unitChangeable; }
  
  /* MODES */

  bool supportAutoMode() const { return this->m_autoMode; }
  bool supportCoolMode() const { return this->m_coolMode; }
  bool supportHeatMode() const { return this->m_heatMode; }
  bool supportDryMode() const { return this->m_dryMode; }

  /* PRESETS */

  bool supportFrostProtectionPreset() const { return this->m_frostProtectionMode; }
  bool supportTurboPreset() const { return this->m_turboCool || this->m_turboHeat; }
  bool supportEcoPreset() const { return this->m_ecoMode || this->m_specialEco; }

  /* SWING MODES */

  bool supportVerticalSwing() const { return this->m_updownFan; }
  bool supportHorizontalSwing() const { return this->m_leftrightFan; }
  bool supportBothSwing() const { return this->m_updownFan && this->m_leftrightFan; }

  /* FAN MODES */

  bool supportSilentFan() const { return this->m_fanSilent; }
  bool supportLowFan() const { return this->m_fanLow; }
  bool supportMediumFan() const { return this->m_fanMedium; }
  bool supportHighFan() const { return this->m_fanHigh; }
  bool supportAutoFan() const { return this->m_fanAuto; }
  bool supportCustomFan() const { return this->m_fanCustom; }

  /* TEMPERATURES */

  float maxTempAuto() const { return this->m_maxTempAuto; }
  float maxTempCool() const { return this->m_maxTempCool; }
  float maxTempHeat() const { return this->m_maxTempHeat; }
  float minTempAuto() const { return this->m_minTempAuto; }
  float minTempCool() const { return this->m_minTempCool; }
  float minTempHeat() const { return this->m_minTempHeat; }

  // Ability to turn LED display off
  bool supportLightControl() const { return this->m_lightControl; }

  bool supportAnion() const { return this->m_anion; }
  bool supportFilterReminder() const { return this->m_filterReminder; }
  bool supportRateSelect() const { return this->m_rateSelect; }
  bool supportRateSelect2Level() const { return this->m_rateSelect2Level; }
  bool supportRateSelect5Level() const { return this->m_rateSelect5Level; }
  bool supportEnergyStats() const { return this->m_powerCal; }
  bool supportEnergySetting() const { return this->m_powerCalSetting; }
  bool supportSound() const { return this->m_sound; }
  bool supportFreshAir() const { return this->has(0x004B); }
  bool supportCascade() const { return this->has(0x0059); }
  bool supportOutdoorSilent() const { return this->has(0x00CD); }
  bool supportIEco() const { return this->has(0x00E3); }

 protected:
  bool m_valid{};
  std::vector<Record> m_records{};
  bool m_updownFan{false};
  bool m_leftrightFan{false};
  bool m_autoMode{false};
  bool m_coolMode{false};
  bool m_dryMode{false};
  bool m_ecoMode{false};
  bool m_specialEco{false};
  bool m_frostProtectionMode{false};
  bool m_heatMode{false};
  bool m_turboCool{false};
  bool m_turboHeat{false};
  bool m_autoSetHumidity{false};
  bool m_activeClean{false};
  bool m_breezeControl{false};
  bool m_buzzer{false};
  bool m_decimals{false};
  bool m_electricAuxHeating{false};
  bool m_fanSpeedControl{true};
  bool m_fanSilent{true};
  bool m_fanLow{true};
  bool m_fanMedium{true};
  bool m_fanHigh{true};
  bool m_fanAuto{true};
  bool m_fanCustom{false};
  bool m_indoorHumidity{false};
  bool m_lightControl{false};
  bool m_manualSetHumidity{false};
  float m_maxTempAuto{30};
  float m_maxTempCool{30};
  float m_maxTempHeat{30};
  float m_minTempAuto{17};
  float m_minTempCool{17};
  float m_minTempHeat{17};
  bool m_oneKeyNoWindOnMe{false};
  bool m_powerCal{false};
  bool m_powerCalSetting{false};
  bool m_energyBcd{false};
  bool m_anion{false};
  bool m_filterReminder{false};
  bool m_rateSelect{false};
  bool m_rateSelect2Level{false};
  bool m_rateSelect5Level{false};
  bool m_sound{false};
  bool m_silkyCool{false};
  bool m_unitChangeable{false};
};

}  // namespace ac
}  // namespace midea
}  // namespace dudanov
