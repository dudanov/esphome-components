#include "esphome/components/light/light_output.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/remote_base/nec_protocol.h"

namespace esphome {
namespace light {
namespace feron {

using esphome::remote_base::NECData;
using esphome::remote_base::RemoteTransmittable;

class FeronLightOutput : public LightOutput, public RemoteTransmittable {
 public:
  LightTraits get_traits() override;
  void write_state(LightState *state) override;

  void preset_load(unsigned preset, bool force = false);
  void preset_save(unsigned preset);

  void set_fade(bool state) { this->fade_ = state; }

 protected:
  void transmit_(uint16_t address, uint16_t command);
  bool fade_{};
};

class FadeSwitch : public switch_::Switch, public Parented<FeronLightOutput> {
 protected:
  void write_state(bool state) override {
    this->publish_state(state);
    this->parent_->set_fade(state);
  }
};

}  // namespace feron
}  // namespace light
}  // namespace esphome
