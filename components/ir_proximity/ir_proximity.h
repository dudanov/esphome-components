#pragma once

#ifdef USE_ESP32
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/esp32_dac_generator/esp32_dac_generator.h"

namespace esphome {
namespace ir_proximity {

using esp32_dac_generator::ChannelsMode;
using esp32_dac_generator::Generator;
using esp32_dac_generator::DACSample;

/// Generate modulated square signal by these PWMs:
/// f = rate / 2, duty: 50% (Ton = Toff ~ 13.2us)
/// f = rate / 128, duty: 50% (Ton = Toff ~ 842us)
/// f = rate / 4096, duty: 50% (Ton = Toff ~ 26.9ms)
template<ChannelsMode mode> class IRLEDModulator : public Generator {
 public:
  IRLEDModulator() : Generator(38000 * 2, 256, 8) { set_mode(mode); }

 protected:
  void write_state(float state) override { this->level_ = static_cast<uint8_t>(state * 255); }

  size_t get_samples(DACSample *samples, const size_t nsamples) override {
    unsigned phase = this->phase_;
    const uint8_t level = this->level_;

    for (unsigned end = phase + nsamples; phase != end; ++samples, ++phase) {
      const uint8_t value = (phase & 0b100001000001) ? 0 : level;

      if (mode == ChannelsMode::I2S_DAC_CHANNEL_LEFT_EN || mode == ChannelsMode::I2S_DAC_CHANNEL_BOTH_EN)
        samples->ch2_left = value;

      if (mode == ChannelsMode::I2S_DAC_CHANNEL_RIGHT_EN || mode == ChannelsMode::I2S_DAC_CHANNEL_BOTH_EN)
        samples->ch1_right = value;
    }

    this->phase_ = phase;
    return nsamples;
  }

  /// Phase of waveform
  unsigned phase_{};
  /// High level of square
  uint8_t level_{};
};

template<ChannelsMode mode>
class IRProximityBinarySensor : public IRLEDModulator<mode>,
                                public remote_base::RemoteReceiverListener,
                                public binary_sensor::BinarySensor,
                                public Component {
 public:
  void setup() override { this->publish_initial_state(false); }
  void loop() override { this->publish_state(false); }
  /// On data from RemoteReceiver
  bool on_receive(remote_base::RemoteReceiveData data) override {
    if (data.size() != 32)
      return false;
    for (unsigned n = 15; n; --n)
      if (!data.expect_item(BIT_TIME_US, BIT_TIME_US))
        return false;
    if (!data.expect_mark(BIT_TIME_US))
      return false;
    this->publish_state(true);
    return true;
  }

 protected:
  static const int32_t BIT_TIME_US = 842;
};

}  // namespace ir_proximity
}  // namespace esphome

#endif  // USE_ESP32
