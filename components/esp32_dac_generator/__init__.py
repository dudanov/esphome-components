from esphome import pins
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import output
from esphome.const import CONF_MODE, CONF_NUMBER, CONF_PIN
from esphome.components.esp32_dac.output import valid_dac_pin

AUTO_LOAD = ["output"]
DEPENDENCIES = ["esp32"]

esp32_dac_generator_ns = cg.esphome_ns.namespace("esp32_dac_generator")
Generator = esp32_dac_generator_ns.class_("Generator", output.FloatOutput)

ChannelsMode = cg.esphome_ns.enum("ChannelsMode")
CHANNEL_MODES = {
    "RIGHT": ChannelsMode.I2S_DAC_CHANNEL_RIGHT_EN,  # Enable I2S built-in DAC right channel, maps to DAC channel 1 on GPIO25
    "LEFT": ChannelsMode.I2S_DAC_CHANNEL_LEFT_EN,    # Enable I2S built-in DAC left  channel, maps to DAC channel 2 on GPIO26
    "BOTH": ChannelsMode.I2S_DAC_CHANNEL_BOTH_EN,    # Enable both of the I2S built-in DAC channels
}
validate_channel_mode = cv.enum(CHANNEL_MODES, upper=True)

DAC_CHANNEL_MAP = {
    25: ChannelsMode.I2S_DAC_CHANNEL_RIGHT_EN,
    26: ChannelsMode.I2S_DAC_CHANNEL_LEFT_EN
}

def get_channel_mode(config, pin, mode):
    return config[mode] if mode in config else DAC_CHANNEL_MAP[config[pin][CONF_NUMBER]]

def pin_to_channel_mode(config, pin):
    return DAC_CHANNEL_MAP[config[pin][CONF_NUMBER]]

def SCHEMA_WITH_PIN(pin):
    return output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Exclusive(pin, "dac_channels_mode"): cv.All(
                pins.internal_gpio_output_pin_schema, valid_dac_pin
            ),
        }
    )

def SCHEMA_WITH_PIN_AND_MODE(pin, mode):
    return SCHEMA_WITH_PIN(pin).extend(
        {
            cv.Exclusive(mode, "dac_channels_mode"): validate_channel_mode,
        }
    )

CONFIG_SCHEMA = cv.Schema({})

async def to_code(config):
    cg.add_global(esp32_dac_generator_ns.using)
