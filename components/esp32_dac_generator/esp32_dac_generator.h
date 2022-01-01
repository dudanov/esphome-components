#pragma once

#ifdef USE_ESP32
#include <type_traits>
#include <cstdint>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s.h>
#include <esphome/components/output/float_output.h>

namespace esphome {
namespace esp32_dac_generator {

#define GEN_TASK_STACKSIZE (configMINIMAL_STACK_SIZE + 256)
#define GEN_TASK_PRIORITY (configMAX_PRIORITIES - 5)

using ChannelsMode = i2s_dac_mode_t;

/// DAC sample struct
struct DACSample {
 protected:
  uint8_t ignored2_;

 public:
  uint8_t ch2_left;

 protected:
  uint8_t ignored1_;

 public:
  uint8_t ch1_right;
};

/// DAC generator class
class Generator : public output::FloatOutput {
 public:
  /// Create generator instance and starts generation of waveform.
  template<typename T> static T *start() {
    static_assert(std::is_base_of<Generator, T>::value, "T must derive from Generator class");
    T *ptr = nullptr;
    if (s_generator == nullptr)
      ptr = new T();
    return ptr;
  }
  static void stop();
  static void set_rate(uint32_t rate) { i2s_set_sample_rates(I2S_NUM_0, rate); }
  static void set_mode(ChannelsMode mode) { i2s_set_dac_mode(mode); }

 protected:
  Generator() = delete;
  Generator(const Generator &) = delete;
  explicit Generator(int rate, int dma_buf_len, int dma_buf_count);
  virtual ~Generator() { stop(); }

  /// Samples generator method. Must be overriden.
  virtual size_t get_samples(DACSample *samples, size_t nsamples) = 0;

 private:
  static Generator *s_generator;
  static DACSample *s_samples;
  static size_t s_nsamples;
  static TaskHandle_t s_task;
  static void s_start(int rate, int dma_buf_len, int dma_buf_count);
  static void gen_task(void *param);
};

#if 0
/// Generate square signal on DAC#1: f = rate / 4, duty: 1/4 and modulated with f = rate / 256, duty: 1/2.
class IRProximitySensor : public Generator {
 public:
  IRProximitySensor() : Generator(38000 * 8, 128, 8) {}
  /// Optional. Overrided for demonstration purposes only.
  void update_frequency(float frequency) override { set_rate(static_cast<uint32_t>(frequency * 8.0f)); }

 protected:
  void write_state(float state) override { this->level_ = static_cast<uint8_t>(state * 255.0f); }
  /// Method for generate samples
  size_t get_samples(DACSample *samples, const size_t nsamples) override {
    for (auto end = samples + nsamples; samples != end; ++samples, ++this->phase_) {
      samples->ch1_right = (this->phase_ & 0x2107) ? 0 : this->level_;
    }
    return nsamples;
  }
  /// Phase counter
  unsigned phase_{};
  /// Output DAC level
  uint8_t level_{};
};
#endif

}  // namespace esp32_dac_generator
}  // namespace esphome

#endif  // USE_ESP32
