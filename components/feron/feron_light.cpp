#include "feron_light.h"

namespace esphome {
namespace light {
namespace feron {

using esphome::remote_base::NECProtocol;

#define FERON_ADDRESS_DIM_BH 0xB721
#define FERON_ADDRESS_SET_BH 0xB781
#define FERON_ADDRESS_NATIVE 0xB7A0
#define FERON_POWER_OFF 0xEC

static const char *const TAG = "feron";

constexpr float kelvin_to_mireds(unsigned k) { return 1e6 / k; }

LightTraits FeronLightOutput::get_traits() {
  LightTraits traits;
  traits.set_supported_color_modes({ColorMode::COLOR_TEMPERATURE});
  traits.set_min_mireds(kelvin_to_mireds(6500));  // 153
  traits.set_max_mireds(kelvin_to_mireds(2700));  // 371
  return traits;
}

void FeronLightOutput::transmit_(uint16_t address, uint16_t command) {
  NECData data = {address, command + ~command * 256};
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2023, 12, 0)
  data.command_repeats = 1;
#endif
  ESP_LOGD(TAG, "Transmitting NEC command: 0x%02X, address: 0x%04X", data.command, data.address);
  RemoteTransmittable::transmit_<NECProtocol>(data);
}

void FeronLightOutput::write_state(LightState *state) {
  float color, brightness;
  state->current_values_as_ct(&color, &brightness);

  // Default to power off command
  uint16_t address = FERON_ADDRESS_NATIVE, command = FERON_POWER_OFF;

  if (brightness > 0.0f) {
    address = this->fade_ ? FERON_ADDRESS_DIM_BH : FERON_ADDRESS_SET_BH;
    command = static_cast<uint16_t>(0.5f + brightness * 15.0f) * 16 + static_cast<uint16_t>(15.5f - color * 15.0f);
  }

  ESP_LOGD(TAG, "Set color: %f, brightness: %f. NEC command: 0x%02X", color, brightness, command);
  this->transmit_(address, command);
}

void FeronLightOutput::preset_load(unsigned preset, bool force) {
  preset %= 16;
  ESP_LOGD(TAG, "Loading state from EEPROM preset: %d", preset);
  this->transmit_(FERON_ADDRESS_NATIVE, preset + force * 16);
}

void FeronLightOutput::preset_save(unsigned preset) {
  preset %= 16;
  ESP_LOGD(TAG, "Saving current state to EEPROM preset: %d", preset);
  this->transmit_(FERON_ADDRESS_NATIVE, preset + 32);
}

}  // namespace feron
}  // namespace light
}  // namespace esphome
