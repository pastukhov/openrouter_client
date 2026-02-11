import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from . import openrouter_ns, OpenRouterComponent

CONF_OPENROUTER_ID = "openrouter_id"
CONF_PUBLISH_STREAMING = "publish_streaming"

OpenRouterTextSensor = openrouter_ns.class_(
    "OpenRouterTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(OpenRouterTextSensor)
    .extend(
        {
            cv.GenerateID(CONF_OPENROUTER_ID): cv.use_id(OpenRouterComponent),
            cv.Optional(CONF_PUBLISH_STREAMING, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_OPENROUTER_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_publish_streaming(config[CONF_PUBLISH_STREAMING]))
