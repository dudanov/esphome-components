#include "i2s_audio_media_player.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "esphome/core/log.h"
#include "pgmspace.h"

#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceSD.h"
#include "AudioOutputI2S.h"
#include "AudioGeneratorMP3.h"

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

I2SAudioMediaPlayer::Scheme I2SAudioMediaPlayer::scheme() const {
  if (this->url_.has_scheme_P(PSTR("http"), PSTR("https")))
    return Scheme::Http;
  return Scheme::None;
}

I2SAudioMediaPlayer::Decoder I2SAudioMediaPlayer::decoder() const {
  if (this->url_.has_extension_P(PSTR("mp3")))
    return Decoder::Mp3;
  if (this->url_.has_extension_P(PSTR("aac")))
    return Decoder::Aac;
  if (this->url_.has_extension_P(PSTR("ogg")))
    return Decoder::Vorbis;
  if (this->url_.has_extension_P(PSTR("opus")))
    return Decoder::Opus;
  if (this->url_.has_extension_P(PSTR("flac")))
    return Decoder::Flac;
  if (this->url_.has_extension_P(PSTR("midi")))
    return Decoder::Midi;
  if (this->url_.has_extension_P(PSTR("mod")))
    return Decoder::Mod;
  if (this->url_.has_extension_P(PSTR("wav")))
    return Decoder::Wave;
  if (this->url_.has_extension_P(PSTR("ay"), PSTR("gbs"), PSTR("gym"), PSTR("hes"), PSTR("kss"), PSTR("nsf"),
                                 PSTR("nsfe"), PSTR("sap"), PSTR("spc"), PSTR("rsn"), PSTR("vgm"), PSTR("vgz"))) {
    return Decoder::Gme;
  }
  return Decoder::None;
}

bool I2SAudioMediaPlayer::open_url(const std::string &url) {
  if (!this->url_.set(url))
    return false;
  const auto scheme = this->scheme();
  if (this->scheme_ == scheme)
    return scheme != Scheme::None;
  if (this->source_ != nullptr) {
    delete this->source_;
    this->source_ = nullptr;
    this->scheme_ = Scheme::None;
  }
  if (scheme == Scheme::Http)
    this->source_ = new AudioFileSourceHTTPStream();
  if (this->source_ == nullptr)
    return false;
  this->scheme_ = scheme;
  return true;
}

void I2SAudioMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (call.get_media_url().has_value()) {
    if (this->open_url(call.get_media_url().value().c_str())) {
      if (this->decoder_->isRunning())
        this->decoder_->stop();
      this->high_freq_.start();
      this->source_->open(this->url_.get().c_str());
      ESP_LOGD(TAG, "Open URL: %s", this->url_.get().c_str());
      this->decoder_->begin(this->source_, this->output_);
      this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
    }
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
      /*      case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
              if (this->audio_->isRunning())
                this->audio_->pauseResume();
              this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
              break;
      */
      case media_player::MEDIA_PLAYER_COMMAND_STOP:
        this->stop_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->mute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->unmute_();
        break;
      /*      case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
              this->audio_->pauseResume();
              if (this->audio_->isRunning()) {
                this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
              } else {
                this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
              }
              break;
      */
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
  this->output_->SetGain(volume);
  if (publish)
    this->volume = volume;
}

void I2SAudioMediaPlayer::stop_() {
  if (this->decoder_->isRunning())
    this->decoder_->stop();
  this->high_freq_.stop();
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
}

void I2SAudioMediaPlayer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Audio...");
  this->decoder_ = new AudioGeneratorMP3();
  if (this->internal_dac_mode_) {
    this->output_ = new AudioOutputI2S(0, 1);
  } else {
    auto i2s = new AudioOutputI2S();
    i2s->SetPinout(this->bclk_pin_, this->lrclk_pin_, this->dout_pin_);
    i2s->SetOutputModeMono(this->external_dac_channels_ == 1);
    this->output_ = i2s;
    if (this->mute_pin_ != nullptr) {
      this->mute_pin_->setup();
      this->mute_pin_->digital_write(false);
    }
  }
  this->output_->begin();
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
}

void I2SAudioMediaPlayer::loop() {
  if (this->decoder_ != nullptr && this->decoder_->isRunning()) {
    if (!this->decoder_->loop())
      this->decoder_->stop();
  }
  if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING && !this->decoder_->isRunning()) {
    this->stop_();
    this->publish_state();
  }
}

media_player::MediaPlayerTraits I2SAudioMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();
  traits.set_supports_pause(false);
  return traits;
};

void I2SAudioMediaPlayer::dump_config() {
  ESP_LOGCONFIG(TAG, "Audio:");
  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "Audio failed to initialize!");
    return;
  }
}

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
