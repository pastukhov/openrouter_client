import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)
from esphome.core import coroutine_with_priority

CODEOWNERS = ["@artem"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["text_sensor"]

CONF_API_KEY = "api_key"
CONF_MODEL = "model"
CONF_TEMPERATURE = "temperature"
CONF_MAX_TOKENS = "max_tokens"
CONF_SYSTEM_ROLE = "system_role"
CONF_ENABLE_STREAMING = "enable_streaming"
CONF_HTTP_TIMEOUT = "http_timeout"
CONF_RESPONSE_BUFFER_SIZE = "response_buffer_size"
CONF_ON_RESPONSE = "on_response"
CONF_ON_ERROR = "on_error"
CONF_ON_STREAMING_CHUNK = "on_streaming_chunk"
CONF_PROMPT = "prompt"

openrouter_ns = cg.esphome_ns.namespace("openrouter")
OpenRouterComponent = openrouter_ns.class_("OpenRouterComponent", cg.Component)

# Triggers
ResponseTrigger = openrouter_ns.class_(
    "ResponseTrigger", automation.Trigger.template(cg.std_string)
)
ErrorTrigger = openrouter_ns.class_(
    "ErrorTrigger", automation.Trigger.template(cg.std_string)
)
StreamingChunkTrigger = openrouter_ns.class_(
    "StreamingChunkTrigger", automation.Trigger.template(cg.std_string)
)

# Actions
AskAction = openrouter_ns.class_("AskAction", automation.Action)
SetModelAction = openrouter_ns.class_("SetModelAction", automation.Action)
SetSystemRoleAction = openrouter_ns.class_("SetSystemRoleAction", automation.Action)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OpenRouterComponent),
        cv.Required(CONF_API_KEY): cv.string,
        cv.Optional(CONF_MODEL, default="openai/gpt-4o-mini"): cv.string,
        cv.Optional(CONF_TEMPERATURE, default=0.7): cv.float_range(min=0.0, max=2.0),
        cv.Optional(CONF_MAX_TOKENS, default=1024): cv.positive_int,
        cv.Optional(CONF_SYSTEM_ROLE): cv.string,
        cv.Optional(CONF_ENABLE_STREAMING, default=False): cv.boolean,
        cv.Optional(CONF_HTTP_TIMEOUT, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RESPONSE_BUFFER_SIZE, default=4096): cv.positive_int,
        cv.Optional(CONF_ON_RESPONSE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ResponseTrigger),
            }
        ),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ErrorTrigger),
            }
        ),
        cv.Optional(CONF_ON_STREAMING_CHUNK): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(StreamingChunkTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(40.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_api_key(config[CONF_API_KEY]))
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_temperature(config[CONF_TEMPERATURE]))
    cg.add(var.set_max_tokens(config[CONF_MAX_TOKENS]))

    if CONF_SYSTEM_ROLE in config:
        cg.add(var.set_system_role(config[CONF_SYSTEM_ROLE]))

    cg.add(var.set_enable_streaming(config[CONF_ENABLE_STREAMING]))
    cg.add(var.set_http_timeout(config[CONF_HTTP_TIMEOUT]))
    cg.add(var.set_response_buffer_size(config[CONF_RESPONSE_BUFFER_SIZE]))

    for conf in config.get(CONF_ON_RESPONSE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "response")], conf)

    for conf in config.get(CONF_ON_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "error")], conf)

    for conf in config.get(CONF_ON_STREAMING_CHUNK, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "chunk")], conf)


# Action: openrouter.ask
ASK_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(OpenRouterComponent),
        cv.Required(CONF_PROMPT): cv.templatable(cv.string),
    }
)


@automation.register_action("openrouter.ask", AskAction, ASK_ACTION_SCHEMA)
async def ask_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template_ = await cg.templatable(config[CONF_PROMPT], args, cg.std_string)
    cg.add(var.set_prompt(template_))

    return var


# Action: openrouter.set_model
SET_MODEL_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(OpenRouterComponent),
        cv.Required(CONF_MODEL): cv.templatable(cv.string),
    }
)


@automation.register_action("openrouter.set_model", SetModelAction, SET_MODEL_ACTION_SCHEMA)
async def set_model_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template_ = await cg.templatable(config[CONF_MODEL], args, cg.std_string)
    cg.add(var.set_model(template_))

    return var


# Action: openrouter.set_system_role
SET_SYSTEM_ROLE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(OpenRouterComponent),
        cv.Required(CONF_SYSTEM_ROLE): cv.templatable(cv.string),
    }
)


@automation.register_action("openrouter.set_system_role", SetSystemRoleAction, SET_SYSTEM_ROLE_ACTION_SCHEMA)
async def set_system_role_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template_ = await cg.templatable(config[CONF_SYSTEM_ROLE], args, cg.std_string)
    cg.add(var.set_system_role(template_))

    return var
