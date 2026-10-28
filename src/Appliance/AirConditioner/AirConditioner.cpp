#include "Appliance/AirConditioner/AirConditioner.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Helpers/Log.h"
#include "Helpers/Timer.h"

namespace dudanov {
namespace midea {
namespace ac {

static const char *TAG = "AirConditioner";

template<typename T>
static bool updateOptional(Optional<T> &property, const T &value) {
  if (property == value)
    return false;
  property = value;
  return true;
}

static bool updateFloat(Optional<float> &property, float value) {
  if (std::isnan(value)) {
    if (!property.hasValue())
      return false;
    property.clear();
    return true;
  }
  return updateOptional(property, value);
}

template<typename T>
static void mergeOptional(Optional<T> &destination, const Optional<T> &source) {
  if (source.hasValue())
    destination = source.value();
}

static bool checkConstraints(Mode mode, Preset preset) {
  if (mode == Mode::MODE_OFF)
    return preset == Preset::PRESET_NONE;
  switch (preset) {
    case Preset::PRESET_NONE:
      return true;
    case Preset::PRESET_ECO:
      return mode == Mode::MODE_COOL;
    case Preset::PRESET_TURBO:
      return mode == Mode::MODE_COOL || mode == Mode::MODE_HEAT;
    case Preset::PRESET_SLEEP:
      return mode != Mode::MODE_DRY && mode != Mode::MODE_FAN_ONLY;
    case Preset::PRESET_FREEZE_PROTECTION:
      return mode == Mode::MODE_HEAT;
    default:
      return false;
  }
}

void AirConditioner::m_setup() {
  if (this->m_autoconfStatus != AUTOCONF_DISABLED)
    this->m_getCapabilities();
  this->m_timerManager.registerTimer(this->m_powerUsageTimer);
  this->m_powerUsageTimer.setCallback([this](Timer *timer) {
    timer->reset();
    if (this->m_powerQuery)
      this->m_getPowerUsage();
  });
  this->m_powerUsageTimer.start(30000);
}

float AirConditioner::getTargetTemp() const {
  return this->m_state.target_temperature.hasValue()
             ? this->m_state.target_temperature.value()
             : std::numeric_limits<float>::quiet_NaN();
}

float AirConditioner::getIndoorTemp() const {
  return this->m_state.indoor_temperature.hasValue()
             ? this->m_state.indoor_temperature.value()
             : std::numeric_limits<float>::quiet_NaN();
}

float AirConditioner::getOutdoorTemp() const {
  return this->m_state.outdoor_temperature.hasValue()
             ? this->m_state.outdoor_temperature.value()
             : std::numeric_limits<float>::quiet_NaN();
}

float AirConditioner::getIndoorHum() const {
  return this->m_state.indoor_humidity.hasValue()
             ? static_cast<float>(this->m_state.indoor_humidity.value())
             : std::numeric_limits<float>::quiet_NaN();
}

float AirConditioner::getPowerUsage() const {
  return this->m_state.realtime_power.hasValue()
             ? this->m_state.realtime_power.value()
             : std::numeric_limits<float>::quiet_NaN();
}

Mode AirConditioner::getMode() const {
  return this->m_state.mode.hasValue() ? this->m_state.mode.value() : Mode::MODE_OFF;
}

SwingMode AirConditioner::getSwingMode() const {
  return this->m_state.swing_mode.hasValue() ? this->m_state.swing_mode.value() : SwingMode::SWING_OFF;
}

FanMode AirConditioner::getFanMode() const {
  return this->m_state.fan_mode.hasValue() ? this->m_state.fan_mode.value() : FanMode::FAN_AUTO;
}

Preset AirConditioner::getPreset() const {
  return this->m_state.preset.hasValue() ? this->m_state.preset.value() : Preset::PRESET_NONE;
}

bool AirConditioner::getApplianceError() const {
  return this->m_state.appliance_error.hasValue() && this->m_state.appliance_error.value();
}

uint8_t AirConditioner::getErrorCode() const {
  return this->m_state.error_code.hasValue() ? this->m_state.error_code.value() : 0;
}

bool AirConditioner::getFilterAlert() const {
  return this->m_state.filter_alert.hasValue() && this->m_state.filter_alert.value();
}

uint32_t AirConditioner::getStateAge() const {
  return this->m_state.valid ? TimerManager::ms() - this->m_state.received_at : UINT32_MAX;
}

void AirConditioner::control(const Control &control) {
  mergeOptional(this->m_pendingControl.mode, control.mode);
  mergeOptional(this->m_pendingControl.preset, control.preset);
  mergeOptional(this->m_pendingControl.fanMode, control.fanMode);
  mergeOptional(this->m_pendingControl.swingMode, control.swingMode);
  mergeOptional(this->m_pendingControl.targetTemp, control.targetTemp);
  mergeOptional(this->m_pendingControl.purifier, control.purifier);
  mergeOptional(this->m_pendingControl.auxHeat, control.auxHeat);
  mergeOptional(this->m_pendingControl.fahrenheit, control.fahrenheit);
  mergeOptional(this->m_pendingControl.targetHumidity, control.targetHumidity);

  if (!this->hasValidatedState()) {
    LOG_D(TAG, "Holding control update until a validated status frame is available...");
    this->m_getStatus();
    return;
  }
  this->m_flushPendingControl();
}

void AirConditioner::m_flushPendingControl() {
  if (this->m_sendControl || !this->hasValidatedState())
    return;

  Control control = this->m_pendingControl;
  this->m_pendingControl = Control{};
  StatusData status = this->m_status.toCommand();
  Mode mode = this->getMode();
  Preset preset = this->getPreset();
  bool has_update = false;
  bool mode_changed = false;

  if (control.mode.hasUpdate(mode)) {
    has_update = true;
    mode_changed = true;
    mode = control.mode.value();
    if (this->getMode() == Mode::MODE_OFF)
      preset = this->m_lastPreset;
    if (!checkConstraints(mode, preset))
      preset = Preset::PRESET_NONE;
  }
  if (control.preset.hasUpdate(preset) && checkConstraints(mode, control.preset.value())) {
    has_update = true;
    preset = control.preset.value();
  }
  if (mode != Mode::MODE_OFF) {
    if (mode == Mode::MODE_AUTO || preset != Preset::PRESET_NONE) {
      if (this->getFanMode() != FanMode::FAN_AUTO) {
        has_update = true;
        status.setFanMode(FanMode::FAN_AUTO);
      }
    } else if (control.fanMode.hasUpdate(this->getFanMode())) {
      has_update = true;
      status.setFanMode(control.fanMode.value());
    }
    if (control.swingMode.hasUpdate(this->getSwingMode())) {
      has_update = true;
      status.setSwingMode(control.swingMode.value());
    }
  }
  if (control.targetTemp.hasUpdate(this->getTargetTemp())) {
    has_update = true;
    status.setTargetTemp(control.targetTemp.value());
  }
  if (control.purifier.hasValue() &&
      (!this->m_state.purifier.hasValue() || control.purifier.value() != this->m_state.purifier.value())) {
    has_update = true;
    status.setPurifier(control.purifier.value());
  }
  if (control.auxHeat.hasValue() &&
      (!this->m_state.aux_heat.hasValue() || control.auxHeat.value() != this->m_state.aux_heat.value())) {
    has_update = true;
    status.setAuxHeat(control.auxHeat.value());
  }
  if (control.fahrenheit.hasValue() &&
      (!this->m_state.fahrenheit.hasValue() || control.fahrenheit.value() != this->m_state.fahrenheit.value())) {
    has_update = true;
    status.setFahrenheits(control.fahrenheit.value());
  }
  if (control.targetHumidity.hasValue() &&
      (!this->m_state.target_humidity.hasValue() ||
       control.targetHumidity.value() != this->m_state.target_humidity.value())) {
    has_update = true;
    status.setTargetHumidity(control.targetHumidity.value());
  }

  if (!has_update)
    return;

  this->m_sendControl = true;
  status.setMode(mode);
  status.setPreset(preset);
  status.setBeeper(this->m_beeper);
  status.appendCRC();

  if (mode_changed && preset != Preset::PRESET_NONE && preset != Preset::PRESET_SLEEP) {
    StatusData final_status = status;
    this->m_setStatus(std::move(final_status));
    status.setPreset(Preset::PRESET_NONE);
    status.setBeeper(false);
    status.updateCRC();
    this->m_queueRequestPriority(
        FrameType::DEVICE_CONTROL, std::move(status),
        std::bind(&AirConditioner::m_readStatus, this, std::placeholders::_1));
  } else {
    this->m_setStatus(std::move(status));
  }
}

void AirConditioner::m_setStatus(StatusData status) {
  LOG_D(TAG, "Enqueuing a priority SET_STATUS(0x40) request...");
  this->m_queueRequestPriority(
      FrameType::DEVICE_CONTROL, std::move(status),
      std::bind(&AirConditioner::m_readStatus, this, std::placeholders::_1),
      [this]() {
        this->m_sendControl = false;
        this->m_flushPendingControl();
      },
      [this]() {
        LOG_W(TAG, "SET_STATUS(0x40) request failed...");
        this->m_sendControl = false;
        this->m_flushPendingControl();
      });
}

void AirConditioner::setPowerState(bool state) {
  if (state == this->getPowerState())
    return;
  Control control;
  Mode mode = this->m_status.getRawMode();
  if (mode < Mode::MODE_AUTO || mode > Mode::MODE_FAN_ONLY)
    mode = Mode::MODE_AUTO;
  control.mode = state ? mode : Mode::MODE_OFF;
  this->control(control);
}

bool AirConditioner::requestGroup(GroupId group, GroupDataCallback on_data, Handler on_error,
                                  uint8_t attempts) {
  const uint8_t index = static_cast<uint8_t>(group);
  const bool known_group = group == GroupId::RUNTIME || group == GroupId::PERFORMANCE ||
                           group == GroupId::INDOOR_FAN || group == GroupId::OUTDOOR_FAN ||
                           group == GroupId::ENERGY || group == GroupId::ENVIRONMENT ||
                           group == GroupId::OUTDOOR_POWER || group == GroupId::LOUVERS;
  if (!known_group || index >= sizeof(this->m_groupPending) || this->m_groupPending[index])
    return false;
  this->m_groupPending[index] = true;
  QueryGroupData data(group);
  LOG_D(TAG, "Enqueuing data group %u request...", index);
  this->m_queueRequest(
      FrameType::DEVICE_QUERY, std::move(data),
      [this, group, on_data](FrameData frame) -> ResponseStatus {
        GroupData result;
        if (!decodeGroupData(frame, group, this->m_powerFormat, result))
          return ResponseStatus::RESPONSE_WRONG;
        this->m_applyGroup(result);
        if (on_data != nullptr)
          on_data(result);
        return ResponseStatus::RESPONSE_OK;
      },
      [this, index]() { this->m_groupPending[index] = false; },
      [this, index, on_error]() {
        this->m_groupPending[index] = false;
        ++this->m_diagnostics.group_failures;
        if (on_error != nullptr)
          on_error();
      },
      attempts);
  return true;
}

void AirConditioner::m_getPowerUsage() {
  this->requestGroup(GroupId::ENERGY, nullptr, nullptr, 1);
}

bool AirConditioner::requestProperties(const std::vector<uint16_t> &ids, PropertiesCallback on_data,
                                       Handler on_error) {
  if (ids.empty() || ids.size() > 120 || this->m_propertyPending)
    return false;
  this->m_propertyPending = true;
  QueryPropertiesData data(ids);
  this->m_queueRequest(
      FrameType::DEVICE_QUERY, std::move(data),
      [this, ids, on_data](FrameData frame) -> ResponseStatus {
        std::vector<PropertyValue> values;
        if (!decodeProperties(frame, values))
          return ResponseStatus::RESPONSE_WRONG;
        for (const auto &value : values) {
          if (std::find(ids.begin(), ids.end(), value.id) == ids.end())
            return ResponseStatus::RESPONSE_WRONG;
        }
        this->m_applyProperties(values);
        if (on_data != nullptr)
          on_data(values);
        return ResponseStatus::RESPONSE_OK;
      },
      [this]() { this->m_propertyPending = false; },
      [this, on_error]() {
        this->m_propertyPending = false;
        ++this->m_diagnostics.property_failures;
        if (on_error != nullptr)
          on_error();
      });
  return true;
}

bool AirConditioner::setProperties(const std::vector<PropertyWrite> &properties,
                                   PropertiesCallback on_data, Handler on_error) {
  size_t encoded_size = 4;
  for (const auto &property : properties) {
    if (property.data.size() > 0xFF)
      return false;
    encoded_size += 3 + property.data.size();
  }
  if (properties.empty() || properties.size() > 0xFF || encoded_size > 245 ||
      this->m_propertyPending)
    return false;
  this->m_propertyPending = true;
  SetPropertiesData data(properties);
  std::vector<uint16_t> ids;
  ids.reserve(properties.size());
  for (const auto &property : properties)
    ids.push_back(property.id);
  this->m_queueRequestPriority(
      FrameType::DEVICE_CONTROL, std::move(data),
      [this, ids, on_data](FrameData frame) -> ResponseStatus {
        std::vector<PropertyValue> values;
        if (!decodeProperties(frame, values))
          return ResponseStatus::RESPONSE_WRONG;
        for (const auto &value : values) {
          if (std::find(ids.begin(), ids.end(), value.id) == ids.end())
            return ResponseStatus::RESPONSE_WRONG;
        }
        this->m_applyProperties(values);
        if (on_data != nullptr)
          on_data(values);
        return ResponseStatus::RESPONSE_OK;
      },
      [this]() { this->m_propertyPending = false; },
      [this, on_error]() {
        this->m_propertyPending = false;
        ++this->m_diagnostics.property_failures;
        if (on_error != nullptr)
          on_error();
      });
  return true;
}

void AirConditioner::m_getCapabilities() {
  GetCapabilitiesData data{};
  this->m_autoconfStatus = AUTOCONF_PROGRESS;
  LOG_D(TAG, "Enqueuing a GET_CAPABILITIES(0xB5) request...");
  this->m_queueRequest(
      FrameType::DEVICE_QUERY, std::move(data),
      [this](FrameData frame) -> ResponseStatus {
        if (!frame.hasID(0xB5) || !frame.hasValidIntegrity())
          return ResponseStatus::RESPONSE_WRONG;
        const bool need_more = this->m_capabilities.read(frame);
        if (!this->m_capabilities.valid())
          return ResponseStatus::RESPONSE_WRONG;
        if (need_more) {
          GetCapabilitiesSecondData second{};
          this->m_sendFrame(FrameType::DEVICE_QUERY, second);
          return ResponseStatus::RESPONSE_PARTIAL;
        }
        return ResponseStatus::RESPONSE_OK;
      },
      [this]() { this->m_autoconfStatus = AUTOCONF_OK; },
      [this]() {
        LOG_W(TAG, "Failed to get 0xB5 capabilities report.");
        this->m_autoconfStatus = AUTOCONF_ERROR;
      });
}

void AirConditioner::m_getStatus() {
  if (this->m_statusPending)
    return;
  this->m_statusPending = true;
  QueryStateData data{};
  LOG_D(TAG, "Enqueuing a GET_STATUS(0x41) request...");
  this->m_queueRequest(
      FrameType::DEVICE_QUERY, std::move(data),
      std::bind(&AirConditioner::m_readStatus, this, std::placeholders::_1),
      [this]() { this->m_statusPending = false; },
      [this]() {
        this->m_statusPending = false;
      });
}

void AirConditioner::m_displayToggle() {
  DisplayToggleData data{};
  LOG_D(TAG, "Enqueuing a priority TOGGLE_LIGHT(0x41) request...");
  this->m_queueRequest(FrameType::DEVICE_QUERY, std::move(data),
                       std::bind(&AirConditioner::m_readStatus, this, std::placeholders::_1));
}

ResponseStatus AirConditioner::m_readStatus(FrameData data) {
  if (!data.hasStatus()) {
    if (data.hasID(0xC0))
      ++this->m_diagnostics.rejected_status_frames;
    return ResponseStatus::RESPONSE_WRONG;
  }

  LOG_D(TAG, "Validated status data received. Parsing...");
  StatusData status_without_integrity{
      FrameData(data.data(), static_cast<uint8_t>(data.size() - 1))};
  this->m_status.copyStatus(status_without_integrity);
  bool changed = false;

  if (status_without_integrity.hasIndex(2)) {
    const Mode mode = status_without_integrity.getMode();
    if (this->m_state.mode.hasValue() && this->m_state.mode.value() != Mode::MODE_OFF &&
        mode == Mode::MODE_OFF && this->m_state.preset.hasValue())
      this->m_lastPreset = this->m_state.preset.value();
    changed |= updateOptional(this->m_state.mode, mode);
    changed |= updateOptional(this->m_state.target_temperature, status_without_integrity.getTargetTemp());
  }
  if (status_without_integrity.hasIndex(3))
    changed |= updateOptional(this->m_state.fan_mode, status_without_integrity.getFanMode());
  if (status_without_integrity.hasIndex(7))
    changed |= updateOptional(this->m_state.swing_mode, status_without_integrity.getSwingMode());
  if (status_without_integrity.hasIndex(10)) {
    changed |= updateOptional(this->m_state.preset, status_without_integrity.getPreset());
    changed |= updateOptional(this->m_state.sleep_mode, status_without_integrity.getSleep());
    changed |= updateOptional(this->m_state.prevent_cold, status_without_integrity.getPreventCold());
    changed |= updateOptional(this->m_state.fahrenheit, status_without_integrity.isFahrenheit());
  }
  if (status_without_integrity.hasIndex(11))
    changed |= updateFloat(this->m_state.indoor_temperature, status_without_integrity.getIndoorTemp());
  if (status_without_integrity.hasIndex(12))
    changed |= updateFloat(this->m_state.outdoor_temperature, status_without_integrity.getOutdoorTemp());
  if (status_without_integrity.hasIndex(1))
    changed |= updateOptional(this->m_state.appliance_error, status_without_integrity.getApplianceError());
  if (status_without_integrity.hasIndex(8)) {
    changed |= updateOptional(this->m_state.c0_byte_8_bit_7,
                              status_without_integrity.getByte8Bit7());
    changed |= updateOptional(this->m_state.power_saving, status_without_integrity.getPowerSaving());
    changed |= updateOptional(this->m_state.smart_eye, status_without_integrity.getSmartEye());
  }
  if (status_without_integrity.hasIndex(9)) {
    changed |= updateOptional(this->m_state.purifier, status_without_integrity.getPurifier());
    changed |= updateOptional(this->m_state.aux_heat, status_without_integrity.getAuxHeat());
    changed |= updateOptional(this->m_state.natural_wind, status_without_integrity.getNaturalWind());
    changed |= updateOptional(this->m_state.c0_byte_9_bit_2,
                              status_without_integrity.getByte9Bit2());
    changed |= updateOptional(this->m_state.eco_mode, status_without_integrity.getEco());
  }
  if (status_without_integrity.hasIndex(13)) {
    changed |= updateOptional(this->m_state.filter_alert, status_without_integrity.getFilterAlert());
    changed |= updateOptional(this->m_state.fresh_filter_timeout,
                              status_without_integrity.getFreshFilterTimeout());
  }
  if (status_without_integrity.hasIndex(14)) {
    changed |= updateOptional(this->m_state.c0_display_field,
                              status_without_integrity.getDisplayField());
    changed |= updateOptional(this->m_state.pmv, status_without_integrity.getPmv());
  }
  if (status_without_integrity.hasIndex(16))
    changed |= updateOptional(this->m_state.error_code, status_without_integrity.getErrorCode());
  if (status_without_integrity.hasIndex(19))
    changed |= updateOptional(this->m_state.target_humidity, status_without_integrity.getTargetHumidity());
  if (status_without_integrity.hasIndex(21))
    changed |= updateOptional(this->m_state.frost_protection,
                              status_without_integrity.getFrostProtection());
  if (status_without_integrity.hasIndex(22))
    changed |= updateOptional(this->m_state.comfort_mode, status_without_integrity.getComfortMode());
  if (status_without_integrity.hasIndex(25))
    changed |= updateOptional(this->m_state.fresh_filter_total,
                              status_without_integrity.getFreshFilterTotal());
  if (status_without_integrity.hasIndex(27))
    changed |= updateOptional(this->m_state.fresh_filter_used,
                              status_without_integrity.getFreshFilterUsed());

  this->m_state.valid = true;
  this->m_state.received_at = TimerManager::ms();
  ++this->m_state.revision;
  ++this->m_diagnostics.valid_status_frames;
  if (changed)
    this->sendUpdate();
  this->m_flushPendingControl();
  return ResponseStatus::RESPONSE_OK;
}

void AirConditioner::m_applyGroup(const GroupData &data) {
  bool changed = false;
  switch (data.group) {
    case GroupId::RUNTIME:
      break;
    case GroupId::PERFORMANCE:
      changed |= updateOptional(this->m_state.compressor_frequency,
                                data.performance.compressor_frequency);
      changed |= updateOptional(this->m_state.target_compressor_frequency,
                                data.performance.target_compressor_frequency);
      changed |= updateOptional(this->m_state.compressor_current, data.performance.compressor_current);
      changed |= updateOptional(this->m_state.compressor_voltage, data.performance.compressor_voltage);
      changed |= updateFloat(this->m_state.indoor_temperature,
                             data.performance.indoor_ambient_temperature);
      changed |= updateFloat(this->m_state.indoor_coil_temperature,
                             data.performance.indoor_coil_temperature);
      changed |= updateFloat(this->m_state.outdoor_coil_temperature,
                             data.performance.outdoor_coil_temperature);
      changed |= updateFloat(this->m_state.outdoor_temperature,
                             data.performance.outdoor_ambient_temperature);
      changed |= updateOptional(this->m_state.discharge_pipe_temperature,
                                data.performance.discharge_pipe_temperature);
      break;
    case GroupId::INDOOR_FAN:
      changed |= updateOptional(this->m_state.target_indoor_fan_speed, data.indoor_fan.target_speed);
      changed |= updateOptional(this->m_state.indoor_fan_speed, data.indoor_fan.actual_speed);
      changed |= updateOptional(this->m_state.water_pump_running,
                                data.indoor_fan.water_pump_running);
      break;
    case GroupId::OUTDOOR_FAN:
      changed |= updateOptional(this->m_state.outdoor_fan_speed, data.outdoor_fan.actual_speed);
      break;
    case GroupId::ENERGY:
      changed |= updateFloat(this->m_state.total_energy, data.energy.total_energy_kwh);
      changed |= updateFloat(this->m_state.total_operating_energy,
                             data.energy.total_operating_energy_kwh);
      changed |= updateFloat(this->m_state.current_energy, data.energy.current_energy_kwh);
      changed |= updateFloat(this->m_state.realtime_power, data.energy.realtime_power_w);
      break;
    case GroupId::ENVIRONMENT:
      if (data.environment.indoor_humidity > 0 && data.environment.indoor_humidity <= 100)
        changed |= updateOptional(this->m_state.indoor_humidity, data.environment.indoor_humidity);
      break;
    case GroupId::OUTDOOR_POWER:
      changed |= updateOptional(this->m_state.outdoor_unit_power, data.outdoor_power.power_w);
      break;
    case GroupId::LOUVERS:
      break;
  }
  if (changed)
    this->sendUpdate();
}

void AirConditioner::m_applyProperties(const std::vector<PropertyValue> &properties) {
  bool changed = false;
  for (const auto &property : properties) {
    auto stored = std::find_if(this->m_properties.begin(), this->m_properties.end(),
                               [&property](const PropertyValue &item) { return item.id == property.id; });
    if (stored == this->m_properties.end())
      this->m_properties.push_back(property);
    else
      *stored = property;
    if (property.data.empty())
      continue;
    const uint8_t value = property.data[0];
    switch (static_cast<PropertyId>(property.id)) {
      case PropertyId::SWING_VERTICAL_ANGLE:
        changed |= updateOptional(this->m_state.vertical_louver_angle, value);
        break;
      case PropertyId::SWING_HORIZONTAL_ANGLE:
        changed |= updateOptional(this->m_state.horizontal_louver_angle, value);
        break;
      case PropertyId::INDOOR_HUMIDITY:
        if (value > 0 && value <= 100)
          changed |= updateOptional(this->m_state.indoor_humidity, value);
        break;
      case PropertyId::DISPLAY_CONTROL:
        changed |= updateOptional(this->m_state.display_on, value != 0);
        break;
      case PropertyId::BREEZELESS:
        changed |= updateOptional(this->m_state.breezeless, value != 0);
        break;
      case PropertyId::PROMPT_TONE:
        changed |= updateOptional(this->m_state.prompt_tone, value != 0);
        break;
      case PropertyId::SELF_CLEAN:
        changed |= updateOptional(this->m_state.self_clean, value != 0);
        break;
      case PropertyId::FILTER_STATUS:
        if (property.data.size() == 5)
          changed |= updateOptional(this->m_state.filter_alert, property.data[4] != 0);
        break;
      case PropertyId::ERROR_CODE:
        changed |= updateOptional(this->m_state.error_code, value);
        break;
      case PropertyId::BREEZE_AWAY:
        changed |= updateOptional(this->m_state.breeze_away, value == 2);
        break;
      case PropertyId::RATE_SELECT:
        changed |= updateOptional(this->m_state.rate_select, value);
        break;
      case PropertyId::FRESH_AIR:
        changed |= updateOptional(
            this->m_state.fresh_air_speed,
            static_cast<uint8_t>(property.data.size() > 1 && value ? property.data[1] : 0));
        break;
      case PropertyId::OUTDOOR_SILENT:
        changed |= updateOptional(this->m_state.outdoor_silent, value == 3);
        break;
      case PropertyId::SOUND:
        changed |= updateOptional(this->m_state.sound, value != 0);
        break;
      default:
        break;
    }
  }
  if (changed)
    this->sendUpdate();
}

const PropertyValue *AirConditioner::getProperty(uint16_t id) const {
  const auto property = std::find_if(this->m_properties.begin(), this->m_properties.end(),
                                     [id](const PropertyValue &item) { return item.id == id; });
  return property == this->m_properties.end() ? nullptr : &*property;
}

}  // namespace ac
}  // namespace midea
}  // namespace dudanov
