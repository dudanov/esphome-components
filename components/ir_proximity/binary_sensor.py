import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.components.esp32_dac_generator as dac
from esphome.const import CONF_ID
from esphome.cpp_generator import TemplateArguments
from esphome.components import (
    binary_sensor,
    output,
    remote_base,
    remote_receiver
)

AUTO_LOAD = ["esp32_dac_generator"]

CONF_LED_PIN = "led_pin"

ir_proximity_ns = cg.esphome_ns.namespace("ir_proximity")

IRProximityBinarySensor = ir_proximity_ns.class_(
    "IRProximityBinarySensor",
    dac.Generator,
    binary_sensor.BinarySensor,
    remote_base.RemoteReceiverListener,
    cg.Component
)


CONFIG_SCHEMA = cv.All(
    binary_sensor.BINARY_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(IRProximityBinarySensor),
            cv.GenerateID(remote_base.CONF_RECEIVER_ID): cv.use_id(
                remote_receiver.RemoteReceiverComponent
            ),
        }
    )
    .extend(dac.SCHEMA_WITH_PIN(CONF_LED_PIN))
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_LED_PIN)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], TemplateArguments(dac.pin_to_channel_mode(config, CONF_LED_PIN)))
    await remote_base.register_listener(var, config)
    await output.register_output(var, config)
    await binary_sensor.register_binary_sensor(var, config)
    await cg.register_component(var, config)
