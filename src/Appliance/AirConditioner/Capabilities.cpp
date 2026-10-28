#include "Appliance/AirConditioner/Capabilities.h"

#include <algorithm>
#include <initializer_list>

#include "Frame/FrameData.h"
#include "Helpers/Log.h"

namespace dudanov {
namespace midea {
namespace ac {

static const char *TAG = "Capabilities";

enum CapabilityID : uint16_t {
  CAPABILITY_INDOOR_HUMIDITY = 0x0015,
  CAPABILITY_SILKY_COOL = 0x0018,
  CAPABILITY_ACTIVE_CLEAN = 0x0039,
  CAPABILITY_ONE_KEY_NO_WIND_ON_ME = 0x0042,
  CAPABILITY_BREEZE_CONTROL = 0x0043,
  CAPABILITY_RATE_SELECT = 0x0048,
  CAPABILITY_FAN_SPEED_CONTROL = 0x0210,
  CAPABILITY_PRESET_ECO = 0x0212,
  CAPABILITY_PRESET_FREEZE_PROTECTION = 0x0213,
  CAPABILITY_MODES = 0x0214,
  CAPABILITY_SWING_MODES = 0x0215,
  CAPABILITY_POWER = 0x0216,
  CAPABILITY_FILTER_REMINDER = 0x0217,
  CAPABILITY_AUX_ELECTRIC_HEATING = 0x0219,
  CAPABILITY_PRESET_TURBO = 0x021A,
  CAPABILITY_ANION = 0x021E,
  CAPABILITY_HUMIDITY = 0x021F,
  CAPABILITY_UNIT_CHANGEABLE = 0x0222,
  CAPABILITY_LIGHT_CONTROL = 0x0224,
  CAPABILITY_TEMPERATURES = 0x0225,
  CAPABILITY_BUZZER = 0x022C,
};

static uint16_t readU16LE(const uint8_t *data) {
  return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
}

static bool contains(uint8_t value, std::initializer_list<uint8_t> values) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

static bool validRange(uint8_t minimum, uint8_t maximum) {
  return minimum <= maximum && maximum <= 100;
}

bool Capabilities::read(const FrameData &frame) {
  this->m_valid = false;
  if (frame.size() < 3 || !frame.hasID(0xB5) || !frame.hasValidIntegrity())
    return false;

  const uint8_t payload_end = static_cast<uint8_t>(frame.size() - 1U);
  const uint8_t count = frame.at(1);
  uint8_t offset = 2;
  std::vector<Record> parsed_records;
  parsed_records.reserve(count);

  for (uint8_t index = 0; index < count; ++index) {
    if (static_cast<uint16_t>(offset) + 3U > payload_end)
      return false;
    Record record;
    record.id = readU16LE(frame.data() + offset);
    const uint8_t length = frame.at(offset + 2);
    offset = static_cast<uint8_t>(offset + 3U);
    if (static_cast<uint16_t>(offset) + length > payload_end)
      return false;
    record.data.assign(frame.data() + offset, frame.data() + offset + length);
    offset = static_cast<uint8_t>(offset + length);
    parsed_records.push_back(std::move(record));
  }

  // B5 replies are seen both with no trailer and with a one-byte message ID.
  // A two-byte trailer is the pagination marker used by the legacy UART path.
  const uint8_t trailer_size = static_cast<uint8_t>(payload_end - offset);
  if (trailer_size > 2)
    return false;
  const bool need_more = trailer_size == 2 && frame.at(offset) != 0;

  for (const auto &record : parsed_records) {
    if (record.data.empty())
      continue;
    const uint8_t value = record.data[0];
    switch (record.id) {
      case CAPABILITY_INDOOR_HUMIDITY:
        this->m_indoorHumidity = value != 0;
        break;
      case CAPABILITY_SILKY_COOL:
        this->m_silkyCool = value != 0;
        break;
      case CAPABILITY_ACTIVE_CLEAN:
        this->m_activeClean = value == 1;
        break;
      case CAPABILITY_ONE_KEY_NO_WIND_ON_ME:
        this->m_oneKeyNoWindOnMe = value == 1;
        break;
      case CAPABILITY_BREEZE_CONTROL:
        this->m_breezeControl = value == 1;
        break;
      case CAPABILITY_RATE_SELECT:
        this->m_rateSelect = value != 0;
        this->m_rateSelect2Level = (value & 0x01) != 0;
        this->m_rateSelect5Level = (value & 0x02) != 0;
        break;
      case CAPABILITY_FAN_SPEED_CONTROL: {
        const bool custom = value == 1;
        this->m_fanSilent = custom || contains(value, {6, 9});
        this->m_fanLow = custom || contains(value, {3, 4, 5, 6, 7, 9});
        this->m_fanMedium = custom || contains(value, {5, 6, 7});
        this->m_fanHigh = custom || contains(value, {3, 4, 5, 6, 7, 9});
        this->m_fanAuto = custom || contains(value, {4, 5, 6, 9});
        this->m_fanCustom = custom;
        this->m_fanSpeedControl = this->m_fanSilent || this->m_fanLow || this->m_fanMedium ||
                                  this->m_fanHigh || this->m_fanAuto;
        break;
      }
      case CAPABILITY_PRESET_ECO:
        this->m_ecoMode = value == 1 || value == 2;
        this->m_specialEco = value == 2;
        break;
      case CAPABILITY_PRESET_FREEZE_PROTECTION:
        this->m_frostProtectionMode = value == 1;
        break;
      case CAPABILITY_MODES:
        if (value <= 13) {
          this->m_heatMode = contains(value, {1, 2, 4, 6, 7, 9, 10, 11, 12, 13});
          this->m_coolMode = !contains(value, {2, 10, 12});
          this->m_dryMode = contains(value, {0, 1, 5, 6, 9, 11, 13});
          this->m_autoMode = contains(value, {0, 1, 2, 7, 8, 9, 13});
        }
        break;
      case CAPABILITY_SWING_MODES:
        if (value <= 3) {
          this->m_leftrightFan = value == 1 || value == 3;
          this->m_updownFan = value < 2;
        }
        break;
      case CAPABILITY_POWER:
        this->m_powerCal = contains(value, {2, 3, 4, 5});
        this->m_powerCalSetting = contains(value, {3, 5});
        this->m_energyBcd = contains(value, {2, 3});
        break;
      case CAPABILITY_FILTER_REMINDER:
        this->m_filterReminder = value != 0;
        break;
      case CAPABILITY_AUX_ELECTRIC_HEATING:
        this->m_electricAuxHeating = value != 0;
        break;
      case CAPABILITY_PRESET_TURBO:
        this->m_turboCool = value < 2;
        this->m_turboHeat = value == 1 || value == 3;
        break;
      case CAPABILITY_ANION:
        this->m_anion = value == 1;
        break;
      case CAPABILITY_HUMIDITY:
        this->m_indoorHumidity = (value & 0x03) != 0;
        this->m_autoSetHumidity = value == 1 || value == 2;
        this->m_manualSetHumidity = value == 2 || value == 3;
        break;
      case CAPABILITY_UNIT_CHANGEABLE:
        this->m_unitChangeable = value == 0;
        break;
      case CAPABILITY_LIGHT_CONTROL:
        this->m_lightControl = contains(value, {1, 2, 100});
        break;
      case CAPABILITY_TEMPERATURES:
        if (record.data.size() >= 6 && validRange(record.data[0], record.data[1]) &&
            validRange(record.data[2], record.data[3]) && validRange(record.data[4], record.data[5])) {
          this->m_minTempCool = static_cast<float>(record.data[0]) * 0.5F;
          this->m_maxTempCool = static_cast<float>(record.data[1]) * 0.5F;
          this->m_minTempAuto = static_cast<float>(record.data[2]) * 0.5F;
          this->m_maxTempAuto = static_cast<float>(record.data[3]) * 0.5F;
          this->m_minTempHeat = static_cast<float>(record.data[4]) * 0.5F;
          this->m_maxTempHeat = static_cast<float>(record.data[5]) * 0.5F;
          this->m_decimals = record.data.size() > 6 && record.data[6] != 0;
        }
        break;
      case CAPABILITY_BUZZER:
        this->m_buzzer = value == 1;
        this->m_sound = value == 1;
        break;
      default:
        break;
    }
  }

  for (auto &record : parsed_records) {
    auto existing = std::find_if(this->m_records.begin(), this->m_records.end(),
                                 [&record](const Record &item) { return item.id == record.id; });
    if (existing == this->m_records.end())
      this->m_records.push_back(std::move(record));
    else
      *existing = std::move(record);
  }

  this->m_valid = true;
  return need_more;
}

const Capabilities::Record *Capabilities::get(uint16_t id) const {
  const auto record = std::find_if(this->m_records.begin(), this->m_records.end(),
                                   [id](const Record &item) { return item.id == id; });
  return record == this->m_records.end() ? nullptr : &*record;
}

bool Capabilities::reported(uint16_t id) const { return this->get(id) != nullptr; }

bool Capabilities::has(uint16_t id) const {
  const Record *record = this->get(id);
  return record != nullptr &&
         std::any_of(record->data.begin(), record->data.end(), [](uint8_t value) { return value != 0; });
}

#define LOG_CAPABILITY(str, condition) \
  if (condition) \
    LOG_CONFIG(TAG, str);

void Capabilities::dump() const {
  LOG_CONFIG(TAG, "CAPABILITIES REPORT:");
  if (this->m_autoMode) {
    LOG_CONFIG(TAG, "  [x] AUTO MODE");
    LOG_CONFIG(TAG, "      - MIN TEMP: %.1f", this->m_minTempAuto);
    LOG_CONFIG(TAG, "      - MAX TEMP: %.1f", this->m_maxTempAuto);
  }
  if (this->m_coolMode) {
    LOG_CONFIG(TAG, "  [x] COOL MODE");
    LOG_CONFIG(TAG, "      - MIN TEMP: %.1f", this->m_minTempCool);
    LOG_CONFIG(TAG, "      - MAX TEMP: %.1f", this->m_maxTempCool);
  }
  if (this->m_heatMode) {
    LOG_CONFIG(TAG, "  [x] HEAT MODE");
    LOG_CONFIG(TAG, "      - MIN TEMP: %.1f", this->m_minTempHeat);
    LOG_CONFIG(TAG, "      - MAX TEMP: %.1f", this->m_maxTempHeat);
  }
  LOG_CAPABILITY("  [x] DRY MODE", this->m_dryMode);
  LOG_CAPABILITY("  [x] ECO MODE", this->m_ecoMode);
  LOG_CAPABILITY("  [x] SPECIAL ECO", this->m_specialEco);
  LOG_CAPABILITY("  [x] FROST PROTECTION MODE", this->m_frostProtectionMode);
  LOG_CAPABILITY("  [x] TURBO COOL", this->m_turboCool);
  LOG_CAPABILITY("  [x] TURBO HEAT", this->m_turboHeat);
  LOG_CAPABILITY("  [x] FAN SILENT", this->m_fanSilent);
  LOG_CAPABILITY("  [x] FAN LOW", this->m_fanLow);
  LOG_CAPABILITY("  [x] FAN MEDIUM", this->m_fanMedium);
  LOG_CAPABILITY("  [x] FAN HIGH", this->m_fanHigh);
  LOG_CAPABILITY("  [x] FAN AUTO", this->m_fanAuto);
  LOG_CAPABILITY("  [x] BREEZE CONTROL", this->m_breezeControl);
  LOG_CAPABILITY("  [x] LIGHT CONTROL", this->m_lightControl);
  LOG_CAPABILITY("  [x] UPDOWN FAN", this->m_updownFan);
  LOG_CAPABILITY("  [x] LEFTRIGHT FAN", this->m_leftrightFan);
  LOG_CAPABILITY("  [x] AUTO SET HUMIDITY", this->m_autoSetHumidity);
  LOG_CAPABILITY("  [x] MANUAL SET HUMIDITY", this->m_manualSetHumidity);
  LOG_CAPABILITY("  [x] INDOOR HUMIDITY", this->m_indoorHumidity);
  LOG_CAPABILITY("  [x] ENERGY STATS", this->m_powerCal);
  LOG_CAPABILITY("  [x] ENERGY SETTING", this->m_powerCalSetting);
  LOG_CAPABILITY("  [x] RATE SELECT", this->m_rateSelect);
  LOG_CAPABILITY("  [x] BUZZER", this->m_buzzer);
  LOG_CAPABILITY("  [x] ACTIVE CLEAN", this->m_activeClean);
  LOG_CAPABILITY("  [x] ELECTRIC AUX HEATING", this->m_electricAuxHeating);
  LOG_CAPABILITY("  [x] FILTER REMINDER", this->m_filterReminder);
  LOG_CAPABILITY("  [x] ANION", this->m_anion);
  LOG_CAPABILITY("  [x] SILKY COOL", this->m_silkyCool);
  LOG_CAPABILITY("  [x] UNIT CHANGEABLE", this->m_unitChangeable);
}

}  // namespace ac
}  // namespace midea
}  // namespace dudanov
