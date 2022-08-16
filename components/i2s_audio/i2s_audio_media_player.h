#pragma once

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "esphome/components/media_player/media_player.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

class AudioFileSource;
class AudioGenerator;
class AudioOutput;

namespace esphome {
namespace i2s_audio {

class UrlSchemeHelper {
 public:
  UrlSchemeHelper() = default;
  UrlSchemeHelper(std::string url) { this->set(std::move(url)); }
  bool set(std::string url);
  const std::string &get() const { return this->url_; }
  const char *ext() const { return &this->url_[this->ext_]; }
  bool has_scheme_P(const char *s) const;
  template<typename... Args> bool has_scheme_P(const char *s, Args... args) const {
    return this->has_scheme_P(s) || this->has_scheme_P(args...);
  }

 protected:
  std::string url_{};
  size_t sch_{0};
  size_t ext_{0};
};

enum UrlScheme {
  None,
  Http,
};

class I2SAudioMediaPlayer : public Component, public media_player::MediaPlayer {
 public:
  void setup() override;
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }

  void loop() override;

  void dump_config() override;

  void set_dout_pin(uint8_t pin) { this->dout_pin_ = pin; }
  void set_bclk_pin(uint8_t pin) { this->bclk_pin_ = pin; }
  void set_lrclk_pin(uint8_t pin) { this->lrclk_pin_ = pin; }
  void set_mute_pin(GPIOPin *mute_pin) { this->mute_pin_ = mute_pin; }
  void set_internal_dac_mode(i2s_dac_mode_t mode) { this->internal_dac_mode_ = mode; }
  void set_external_dac_channels(uint8_t channels) { this->external_dac_channels_ = channels; }

  media_player::MediaPlayerTraits get_traits() override;

  bool is_muted() const override { return this->muted_; }

 protected:
  void control(const media_player::MediaPlayerCall &call) override;

  void mute_();
  void unmute_();
  void set_volume_(float volume, bool publish = true);
  void stop_();

  bool open_url(const std::string &url);

  AudioFileSource *source_;
  AudioGenerator *decoder_;
  AudioOutput *output_;
  UrlScheme scheme_{None};

  uint8_t dout_pin_{0};
  uint8_t din_pin_{0};
  uint8_t bclk_pin_;
  uint8_t lrclk_pin_;

  GPIOPin *mute_pin_{nullptr};
  bool muted_{false};
  float unmuted_volume_{0};

  i2s_dac_mode_t internal_dac_mode_{I2S_DAC_CHANNEL_DISABLE};
  uint8_t external_dac_channels_;

  HighFrequencyLoopRequester high_freq_;
};

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
