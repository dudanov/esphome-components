#include "i2s_audio_media_player.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "esphome/core/log.h"
#include "pgmspace.h"

#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioOutputI2S.h"
#include "AudioGeneratorAAC.h"
#include "AudioGeneratorFLAC.h"
#include "AudioGeneratorGme.h"
#include "AudioGeneratorMIDI.h"
#include "AudioGeneratorMOD.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorOpus.h"
#include "AudioGeneratorWAV.h"

namespace esphome {
namespace i2s_audio {

static const char *const TAG = "audio";

bool Url::set(const std::string &url) {
  auto p = strstr_P(url.c_str(), PSTR("://"));
  if (p == nullptr)
    return false;
  auto sch = p - url.c_str();
  if (sch < 2)
    return false;
  auto num = url.rfind('#');
  if (num != std::string::npos)
    this->track_ = atoi(&url[num + 1]);
  auto ext = url.rfind('.');
  if (ext == std::string::npos || sch > ext)
    return false;
  if ((ext - sch) < 4 || (url.size() - ext) < 3)
    return false;
  this->url_ = url.substr(0, num);
  this->sch_ = sch;
  this->ext_ = ext + 1;
  return true;
}

I2SAudioMediaPlayer::Scheme I2SAudioMediaPlayer::scheme() const {
  if (this->url_.has_scheme_P(PSTR("http"), PSTR("https")))
    return SCHEME_HTTP;
  return SCHEME_NONE;
}

I2SAudioMediaPlayer::Decoder I2SAudioMediaPlayer::decoder() const {
  if (this->url_.has_extension_P(PSTR("mp3")))
    return DECODER_MP3;
  if (this->url_.has_extension_P(PSTR("aac")))
    return DECODER_AAC;
  if (this->url_.has_extension_P(PSTR("opus")))
    return DECODER_OPUS;
  if (this->url_.has_extension_P(PSTR("flac")))
    return DECODER_FLAC;
  if (this->url_.has_extension_P(PSTR("midi")))
    return DECODER_MIDI;
  if (this->url_.has_extension_P(PSTR("mod")))
    return DECODER_MOD;
  if (this->url_.has_extension_P(PSTR("wav")))
    return DECODER_WAVE;
  if (this->url_.has_extension_P(PSTR("ay"), PSTR("gbs"), PSTR("gym"), PSTR("hes"), PSTR("kss"), PSTR("nsf"),
                                 PSTR("nsfe"), PSTR("sap"), PSTR("spc"), PSTR("rsn"), PSTR("vgm"), PSTR("vgz"))) {
    return DECODER_GME;
  }
  return DECODER_NONE;
}

bool I2SAudioMediaPlayer::open_url(const std::string &url) {
  Url n;
  if (!n.set(url))
    return false;
  
  if (this->url_.get() == n.get()) {
    if (this->url_.track() != n.track() || this->state != media_player::MEDIA_PLAYER_STATE_PLAYING) {
      this->url_ = std::move(n);
      return true;
    }
    return false;
  }
  
  this->url_ = std::move(n);

  const auto scheme = this->scheme();
  if (scheme == SCHEME_NONE)
    return false;
  const auto decoder = this->decoder();
  if (decoder == DECODER_NONE)
    return false;
  if (this->generator_ != nullptr && this->generator_->isRunning())
    this->generator_->stop();
  if (this->decoder_ != decoder) {
    if (this->generator_ != nullptr) {
      delete this->generator_;
      this->generator_ = nullptr;
      this->decoder_ = DECODER_NONE;
    }
    if (decoder == DECODER_MP3)
      this->generator_ = new AudioGeneratorMP3();
    else if (decoder == DECODER_AAC)
      this->generator_ = new AudioGeneratorAAC();
    else if (decoder == DECODER_OPUS)
      this->generator_ = new AudioGeneratorOpus();
    else if (decoder == DECODER_FLAC)
      this->generator_ = new AudioGeneratorFLAC();
    else if (decoder == DECODER_MIDI)
      this->generator_ = new AudioGeneratorMIDI();
    else if (decoder == DECODER_MOD)
      this->generator_ = new AudioGeneratorMOD();
    else if (decoder == DECODER_WAVE)
      this->generator_ = new AudioGeneratorWAV();
    else if (decoder == DECODER_GME)
      this->generator_ = new AudioGeneratorGme();
  }
  if (this->generator_ == nullptr)
    return false;
  this->decoder_ = decoder;
  if (this->source_ != nullptr && this->source_->isOpen())
    this->source_->close();
  if (this->scheme_ != scheme) {
    if (this->source_ != nullptr) {
      delete this->source_;
      this->source_ = nullptr;
      this->scheme_ = SCHEME_NONE;
    }
    if (scheme == SCHEME_HTTP)
      this->source_ = new AudioFileSourceHTTPStream();
  }
  if (this->source_ == nullptr)
    return false;
  this->scheme_ = scheme;
  this->generator_->RegisterStatusCB(
      [](void *, int code, const char *msg) { ESP_LOGD(TAG, "Status: %d, %s", code, msg); }, nullptr);
  this->generator_->RegisterMetadataCB(
      [](void *, const char *type, bool, const char *msg) { ESP_LOGD(TAG, "Metadata: %s = %s", type, msg); }, nullptr);
  // if (this->buffer_ != nullptr)
  // delete this->buffer_;
  // this->buffer_ = new AudioFileSourceBuffer(this->source_, 4096);
  this->source_->open(url.c_str());
  this->generator_->begin(this->source_, this->output_);
  this->high_freq_.start();
  return true;
}

void I2SAudioMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (call.get_media_url().has_value()) {
    if (this->open_url(call.get_media_url().value().c_str())) {
      // if (this->generator_->isRunning())
      // this->generator_->stop();
      // this->source_->open(this->url_.get().c_str());
      ESP_LOGD(TAG, "Open URL: %s", this->url_.get().c_str());
      // this->output_->SetRate(48000);
      // this->generator_->begin(this->source_, this->output_);
      ESP_LOGD(TAG, "Size URL: %d", this->source_->getSize());

      if (this->decoder_ == DECODER_GME)
        ((AudioGeneratorGme *) this->generator_)->PlayTrack(this->url_.track());
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
        // if (!this->generator_->isRunning())
        // this->generator_->begin(this->source_, this->output_);
        // if (this->decoder_ == DECODER_GME)
        // static_cast<AudioGeneratorGme *>(this->generator_)->PlayTrack(0);
        // this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
        //        if (this->audio_->isRunning())
        //         this->audio_->pauseResume();
        //     this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_STOP:
        // this->stop_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        // this->mute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        // this->unmute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
        //        this->audio_->pauseResume();
        //      if (this->audio_->isRunning()) {
        //      this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
        //        } else {
        //        this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
        //     }
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
  this->output_->SetGain(volume);
  if (publish)
    this->volume = volume;
}

void I2SAudioMediaPlayer::stop_() {
  if (this->generator_->isRunning())
    this->generator_->stop();
  this->high_freq_.stop();
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
}

void I2SAudioMediaPlayer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Audio...");
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
  if (this->generator_ != nullptr) {
    this->generator_->loop();
    if (!this->generator_->isRunning()) {
      this->generator_->stop();
      this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
    }
  }
  // if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING && !this->generator_->isRunning()) {
  // this->stop_();
  // this->publish_state();
  //}
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
