#ifdef USE_ESP32
#include <cstring>
#include "esp32_dac_generator.h"

namespace esphome {
namespace esp32_dac_generator {

Generator *Generator::s_generator;
DACSample *Generator::s_samples;
size_t Generator::s_nsamples;
TaskHandle_t Generator::s_task;

Generator::Generator(int rate, int dma_buf_len, int dma_buf_count) {
  if (s_generator != nullptr)
    stop();
  s_generator = this;
  s_start(rate, dma_buf_len, dma_buf_count);
}

void Generator::s_start(int rate, int dma_buf_len, int dma_buf_count) {
  if (s_samples != nullptr)
    return;
  s_nsamples = dma_buf_len * dma_buf_count;
  s_samples = new DACSample[s_nsamples];
  if (s_samples == nullptr)
    return;
  memset(s_samples, 0, s_nsamples * sizeof(DACSample));
  i2s_config_t i2s_config = {.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
                             .sample_rate = rate,
                             .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
                             .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
                             .communication_format = I2S_COMM_FORMAT_I2S_MSB,
                             .intr_alloc_flags = 0,
                             .dma_buf_count = dma_buf_count,
                             .dma_buf_len = dma_buf_len,
                             .use_apll = false,
                             .tx_desc_auto_clear = false,
                             .fixed_mclk = 0};
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, nullptr);
  xTaskCreate(gen_task, "esp32_dac_generator", GEN_TASK_STACKSIZE, nullptr, GEN_TASK_PRIORITY, &s_task);
}

void Generator::stop() {
  if (s_samples == nullptr)
    return;
  vTaskDelete(s_task);
  i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
  i2s_driver_uninstall(I2S_NUM_0);
  delete[] s_samples;
  s_samples = nullptr;
  delete s_generator;
  s_generator = nullptr;
}

void Generator::gen_task(void *param) {
  while (true) {
    size_t written = s_generator->get_samples(s_samples, s_nsamples) * sizeof(DACSample);
    i2s_write(I2S_NUM_0, s_samples, written, &written, portMAX_DELAY);
  }
}

}  // namespace esp32_dac_generator
}  // namespace esphome

#endif  // USE_ESP32
