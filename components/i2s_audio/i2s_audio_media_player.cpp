#include "i2s_audio_media_player.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "esphome/core/log.h"
#include "pgmspace.h"

#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceSD.h"

namespace esphome {
namespace i2s_audio {

static const char *const TAG = "audio";

bool Url::set(const std::string &url) {
  if (this->url_ == url)
    return false;
  auto p = strstr_P(url.c_str(), PSTR("://"));
  if (p == nullptr)
    return false;
  auto sch = p - url.c_str();
  if (sch < 2)
    return false;
  auto ext = url.rfind('.');
  if (ext == std::string::npos || sch > ext)
    return false;
  if ((ext - sch) < 4 || (url.size() - ext) < 3)
    return false;
  this->url_ = url;
  this->sch_ = sch;
  this->ext_ = ext + 1;
  return true;
}

Url::Scheme Url::get_scheme() const {
  if (this->has_scheme_P(PSTR("http"), PSTR("https")))
    return Url::Scheme::Http;
  return Url::Scheme::None;
}

bool I2SAudioMediaPlayer::open_url(const std::string &url) {
  if (!this->url_.set(url))
    return false;
  const auto scheme = this->url_.get_scheme();
  if (this->scheme_ == scheme)
    return scheme != Url::Scheme::None;
  if (this->source_ != nullptr) {
    delete this->source_;
    this->source_ = nullptr;
    this->scheme_ = Url::Scheme::None;
  }
  if (scheme == Url::Scheme::Http)
    this->source_ = new AudioFileSourceHTTPStream();
  if (this->source_ == nullptr)
    return false;
  this->scheme_ = scheme;
  return true;
}

void I2SAudioMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (call.get_media_url().has_value()) {
    if (this->decoder_->isRunning())
      this->audio_->stopSong();
    this->high_freq_.start();
    this->audio_->connecttohost(call.get_media_url().value().c_str());
    this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
  }
  if (call.get_volume().has_value()) {
    this->volume = call.get_volume().value();
    this->set_volume_(volume);
    this->unmute_();
  }
  if (call.get_command().has_value()) {
    switch (call.get_command().value()) {
      case media_player::MEDIA_PLAYER_COMMAND_PLAY:
        if (!this->decoder_->isRunning())
          this->decoder_->begin(this->source_, this->output_);
        this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
        if (this->audio_->isRunning())
          this->audio_->pauseResume();
        this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_STOP:
        this->stop_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->mute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->unmute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
        this->audio_->pauseResume();
        if (this->audio_->isRunning()) {
          this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
        } else {
          this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
        }
        break;
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP: {
        float new_volume = this->volume + 0.1f;
        if (new_volume > 1.0f)
          new_volume = 1.0f;
        this->set_volume_(new_volume);
        this->unmute_();
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN: {
        float new_volume = this->volume - 0.1f;
        if (new_volume < 0.0f)
          new_volume = 0.0f;
        this->set_volume_(new_volume);
        this->unmute_();
        break;
      }
    }
  }
  this->publish_state();
}

void I2SAudioMediaPlayer::mute_() {
  if (this->mute_pin_ != nullptr) {
    this->mute_pin_->digital_write(true);
  } else {
    this->set_volume_(0.0f, false);
  }
  this->muted_ = true;
}
void I2SAudioMediaPlayer::unmute_() {
  if (this->mute_pin_ != nullptr) {
    this->mute_pin_->digital_write(false);
  } else {
    this->set_volume_(this->volume, false);
  }
  this->muted_ = false;
}
void I2SAudioMediaPlayer::set_volume_(float volume, bool publish) {
  this->audio_->setVolume(remap<uint8_t, float>(volume, 0.0f, 1.0f, 0, 21));
  if (publish)
    this->volume = volume;
}

void I2SAudioMediaPlayer::stop_() {
  if (this->audio_->isRunning())
    this->audio_->stopSong();
  this->high_freq_.stop();
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
}

void I2SAudioMediaPlayer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Audio...");
  if (this->internal_dac_mode_ != I2S_DAC_CHANNEL_DISABLE) {
    this->audio_ = make_unique<Audio>(true, this->internal_dac_mode_);
  } else {
    this->audio_ = make_unique<Audio>(false);
    this->audio_->setPinout(this->bclk_pin_, this->lrclk_pin_, this->dout_pin_);
    this->audio_->forceMono(this->external_dac_channels_ == 1);
    if (this->mute_pin_ != nullptr) {
      this->mute_pin_->setup();
      this->mute_pin_->digital_write(false);
    }
  }
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
}

void I2SAudioMediaPlayer::loop() {
  this->audio_->loop();
  if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING && !this->audio_->isRunning()) {
    this->stop_();
    this->publish_state();
  }
}

media_player::MediaPlayerTraits I2SAudioMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();
  traits.set_supports_pause(true);
  return traits;
};

void I2SAudioMediaPlayer::dump_config() {
  ESP_LOGCONFIG(TAG, "Audio:");
  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "Audio failed to initialize!");
    return;
  }
  if (this->internal_dac_mode_ != I2S_DAC_CHANNEL_DISABLE) {
    switch (this->internal_dac_mode_) {
      case I2S_DAC_CHANNEL_LEFT_EN:
        ESP_LOGCONFIG(TAG, "  Internal DAC mode: Left");
        break;
      case I2S_DAC_CHANNEL_RIGHT_EN:
        ESP_LOGCONFIG(TAG, "  Internal DAC mode: Right");
        break;
      case I2S_DAC_CHANNEL_BOTH_EN:
        ESP_LOGCONFIG(TAG, "  Internal DAC mode: Left & Right");
        break;
      default:
        break;
    }
  }
}

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
