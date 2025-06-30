import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, remote_base, switch
from esphome.const import CONF_OUTPUT_ID, DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG

AUTO_LOAD = ["light", "switch"]
CONF_FADE_SWITCH = "fade_switch"
ICON_FADE = "mdi:animation-play-outline"


feron_ns = light.light_ns.namespace("feron")

FeronLightOutput = feron_ns.class_(
    "FeronLightOutput",
    light.LightOutput,
    remote_base.RemoteTransmittable,
)

FadeSwitch = feron_ns.class_("FadeSwitch", switch.Switch, cg.Parented)


CONFIG_SCHEMA = light.LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(FeronLightOutput),
        cv.Optional(CONF_FADE_SWITCH): switch.switch_schema(
            FadeSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_FADE,
            default_restore_mode="RESTORE_DEFAULT_ON",
        ),
    }
).extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA)


async def to_code(config):
    output = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await remote_base.register_transmittable(output, config)
    await light.register_light(output, config)

    if fs := config.get(CONF_FADE_SWITCH):
        s = await switch.new_switch(fs)
        await cg.register_parented(s, output)
