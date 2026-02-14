#include "esp_err.h"
#include "openrouter.h"
#include "unity.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static char *test_function_callback(const char *function_name, const char *arguments, void *user_data) {
  (void) function_name;
  (void) arguments;
  (void) user_data;
  const char *json = "{\"ok\":true}";
  size_t len = strlen(json);
  char *result = (char *) malloc(len + 1);
  if (result == NULL) {
    return NULL;
  }
  memcpy(result, json, len + 1);
  return result;
}

TEST_CASE("openrouter_create rejects null config", "[openrouter]") {
  TEST_ASSERT_NULL(openrouter_create(NULL));
}

TEST_CASE("openrouter_create rejects missing api key", "[openrouter]") {
  openrouter_config_t config = {
      .api_key = NULL,
      .enable_streaming = false,
  };
  TEST_ASSERT_NULL(openrouter_create(&config));
}

TEST_CASE("openrouter_create uses defaults and can be destroyed", "[openrouter]") {
  openrouter_config_t config = {
      .api_key = "test-api-key",
      .enable_streaming = false,
  };

  openrouter_handle_t handle = openrouter_create(&config);
  TEST_ASSERT_NOT_NULL(handle);
  openrouter_destroy(handle);
}

TEST_CASE("openrouter setters validate argument ranges", "[openrouter]") {
  openrouter_config_t config = {
      .api_key = "test-api-key",
      .enable_streaming = false,
  };
  openrouter_handle_t handle = openrouter_create(&config);
  TEST_ASSERT_NOT_NULL(handle);

  TEST_ASSERT_EQUAL(ESP_OK, openrouter_set_temperature(handle, 0.0f));
  TEST_ASSERT_EQUAL(ESP_OK, openrouter_set_temperature(handle, 2.0f));
  TEST_ASSERT_NOT_EQUAL(ESP_OK, openrouter_set_temperature(handle, -0.1f));
  TEST_ASSERT_NOT_EQUAL(ESP_OK, openrouter_set_temperature(handle, 2.1f));

  TEST_ASSERT_EQUAL(ESP_OK, openrouter_set_top_p(handle, 0.0f));
  TEST_ASSERT_EQUAL(ESP_OK, openrouter_set_top_p(handle, 1.0f));
  TEST_ASSERT_NOT_EQUAL(ESP_OK, openrouter_set_top_p(handle, -0.01f));
  TEST_ASSERT_NOT_EQUAL(ESP_OK, openrouter_set_top_p(handle, 1.01f));

  TEST_ASSERT_EQUAL(ESP_OK, openrouter_set_max_tokens(handle, 1));
  TEST_ASSERT_NOT_EQUAL(ESP_OK, openrouter_set_max_tokens(handle, 0));

  openrouter_destroy(handle);
}

TEST_CASE("openrouter register and unregister simple function", "[openrouter]") {
  const char *units[] = {"celsius", "fahrenheit", NULL};
  openrouter_param_t params[] = {
      {.name = "location", .type = "string", .description = "Location name", .required = true, .enum_values = NULL},
      {.name = "unit", .type = "string", .description = "Temperature unit", .required = false, .enum_values = units},
      {.name = NULL, .type = NULL, .description = NULL, .required = false, .enum_values = NULL},
  };

  openrouter_simple_function_t function = {
      .name = "get_weather",
      .description = "Return test weather payload",
      .parameters = params,
      .callback = test_function_callback,
      .user_data = NULL,
  };

  openrouter_config_t config = {
      .api_key = "test-api-key",
      .enable_streaming = false,
      .enable_tools = true,
  };
  openrouter_handle_t handle = openrouter_create(&config);
  TEST_ASSERT_NOT_NULL(handle);

  TEST_ASSERT_EQUAL(ESP_OK, openrouter_register_simple_function(handle, &function));
  TEST_ASSERT_NOT_EQUAL(ESP_OK, openrouter_register_simple_function(handle, &function));
  TEST_ASSERT_EQUAL(ESP_OK, openrouter_unregister_function(handle, "get_weather"));
  TEST_ASSERT_NOT_EQUAL(ESP_OK, openrouter_unregister_function(handle, "get_weather"));
  TEST_ASSERT_EQUAL(ESP_OK, openrouter_clear_functions(handle));

  openrouter_destroy(handle);
}
