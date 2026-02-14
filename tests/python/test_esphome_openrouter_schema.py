from pathlib import Path
import sys

import pytest
import esphome.config_validation as cv
from esphome.const import CONF_ID

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from components.openrouter import (  # noqa: E402
    ASK_ACTION_SCHEMA,
    CONFIG_SCHEMA,
    SET_MODEL_ACTION_SCHEMA,
    SET_SYSTEM_ROLE_ACTION_SCHEMA,
)


def test_openrouter_config_defaults_are_applied() -> None:
    cfg = CONFIG_SCHEMA({CONF_ID: "ai_agent", "api_key": "test-key"})
    assert cfg["model"] == "openai/gpt-4o-mini"
    assert cfg["temperature"] == 0.7
    assert cfg["max_tokens"] == 1024
    assert cfg["enable_streaming"] is False
    assert cfg["response_buffer_size"] == 4096


def test_openrouter_rejects_temperature_out_of_range() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({CONF_ID: "ai_agent", "api_key": "test-key", "temperature": 2.01})


def test_openrouter_rejects_negative_http_timeout() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({CONF_ID: "ai_agent", "api_key": "test-key", "http_timeout": "-1s"})


def test_ask_action_schema_accepts_prompt_template_string() -> None:
    cfg = ASK_ACTION_SCHEMA({CONF_ID: "ai_agent", "prompt": "Hello from test"})
    assert cfg["prompt"] == "Hello from test"


def test_set_model_action_schema_accepts_model_string() -> None:
    cfg = SET_MODEL_ACTION_SCHEMA({CONF_ID: "ai_agent", "model": "openai/gpt-4o-mini"})
    assert cfg["model"] == "openai/gpt-4o-mini"


def test_set_system_role_action_schema_accepts_role_string() -> None:
    cfg = SET_SYSTEM_ROLE_ACTION_SCHEMA(
        {CONF_ID: "ai_agent", "system_role": "You are a helpful assistant."}
    )
    assert cfg["system_role"] == "You are a helpful assistant."
