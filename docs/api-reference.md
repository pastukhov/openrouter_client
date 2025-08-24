# API Reference

This document provides a comprehensive reference for all functions, constants, and data structures in the OpenRouter ESP-IDF library.

## Table of Contents

- [Handle Management](#handle-management)
- [Configuration Functions](#configuration-functions)
- [Basic API Calls](#basic-api-calls)
- [Streaming API Calls](#streaming-api-calls)
- [Function Calling (Tools)](#function-calling-tools)
- [Multimodal API Calls](#multimodal-api-calls)
- [Utility Functions](#utility-functions)
- [Constants and Macros](#constants-and-macros)

---

## Handle Management

### openrouter_create

```c
openrouter_handle_t openrouter_create(const openrouter_config_t *config);
```

Creates a new OpenRouter client handle with the specified configuration.

**Parameters:**
- `config` - Pointer to configuration structure containing API key, model, and other settings

**Returns:**
- `openrouter_handle_t` - Handle on success, `NULL` on failure

**Example:**
```c
openrouter_config_t config = {
    .api_key = "your_api_key",
    .default_model = "openai/gpt-3.5-turbo",
    .temperature = 0.7f,
    .max_tokens = 1024,
    .response_buffer_size = 4096,
    .enable_streaming = false
};

openrouter_handle_t handle = openrouter_create(&config);
if (!handle) {
    ESP_LOGE(TAG, "Failed to create OpenRouter handle");
    return ESP_FAIL;
}
```

**Notes:**
- All string parameters in config are copied internally
- The returned handle must be freed with `openrouter_destroy()`
- Configuration values are validated and defaults applied for missing optional parameters

---

### openrouter_destroy

```c
void openrouter_destroy(openrouter_handle_t handle);
```

Destroys an OpenRouter client handle and frees all associated resources.

**Parameters:**
- `handle` - OpenRouter handle to destroy

**Example:**
```c
openrouter_destroy(handle);
handle = NULL; // Good practice to avoid use-after-free
```

**Notes:**
- Safe to call with `NULL` handle
- Automatically cleans up registered functions, cached connections, and mutexes
- Should be called when the handle is no longer needed

---

## Configuration Functions

### openrouter_set_model

```c
esp_err_t openrouter_set_model(openrouter_handle_t handle, const char *model);
```

Sets the AI model to use for API calls.

**Parameters:**
- `handle` - OpenRouter handle
- `model` - Model name (e.g., "openai/gpt-4", "anthropic/claude-3-sonnet")

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_INVALID_ARG` if handle or model is NULL
- `ESP_ERR_NO_MEM` if memory allocation fails

**Example:**
```c
esp_err_t err = openrouter_set_model(handle, "openai/gpt-4");
if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set model: %s", esp_err_to_name(err));
}
```

---

### openrouter_set_system_role

```c
esp_err_t openrouter_set_system_role(openrouter_handle_t handle, const char *role);
```

Sets the system role message that defines the AI's behavior.

**Parameters:**
- `handle` - OpenRouter handle
- `role` - System role message (NULL to clear)

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_INVALID_ARG` if handle is NULL
- `ESP_ERR_NO_MEM` if memory allocation fails

**Example:**
```c
esp_err_t err = openrouter_set_system_role(handle, 
    "You are a helpful IoT assistant specialized in ESP32 development.");
```

---

### openrouter_set_temperature

```c
esp_err_t openrouter_set_temperature(openrouter_handle_t handle, float temperature);
```

Sets the temperature parameter controlling response randomness.

**Parameters:**
- `handle` - OpenRouter handle
- `temperature` - Temperature value (0.0-2.0)
  - `0.0` - Most focused and deterministic
  - `1.0` - Balanced creativity
  - `2.0` - Most creative and random

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_INVALID_ARG` if handle is NULL or temperature is out of range

**Example:**
```c
// More creative responses
openrouter_set_temperature(handle, 1.2f);

// More focused responses
openrouter_set_temperature(handle, 0.2f);
```

---

### openrouter_set_max_tokens

```c
esp_err_t openrouter_set_max_tokens(openrouter_handle_t handle, int max_tokens);
```

Sets the maximum number of tokens to generate in responses.

**Parameters:**
- `handle` - OpenRouter handle
- `max_tokens` - Maximum tokens (must be > 0)

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_INVALID_ARG` if handle is NULL or max_tokens <= 0

**Example:**
```c
// Longer responses
openrouter_set_max_tokens(handle, 2048);

// Shorter responses to save costs
openrouter_set_max_tokens(handle, 512);
```

---

### openrouter_set_top_p

```c
esp_err_t openrouter_set_top_p(openrouter_handle_t handle, float top_p);
```

Sets the top_p (nucleus sampling) parameter for controlling response diversity.

**Parameters:**
- `handle` - OpenRouter handle
- `top_p` - Top_p value (0.0-1.0)
  - `0.1` - Only top 10% probability tokens considered
  - `1.0` - All tokens considered

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_INVALID_ARG` if handle is NULL or top_p is out of range

**Example:**
```c
// More focused sampling
openrouter_set_top_p(handle, 0.9f);
```

---

### openrouter_set_seed

```c
esp_err_t openrouter_set_seed(openrouter_handle_t handle, int seed);
```

Sets the random seed for deterministic responses.

**Parameters:**
- `handle` - OpenRouter handle
- `seed` - Seed value (-1 for random behavior)

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_INVALID_ARG` if handle is NULL

**Example:**
```c
// Deterministic responses
openrouter_set_seed(handle, 12345);

// Random behavior
openrouter_set_seed(handle, -1);
```

---

### openrouter_set_streaming

```c
esp_err_t openrouter_set_streaming(openrouter_handle_t handle, bool enable_streaming);
```

Enables or disables streaming mode.

**Parameters:**
- `handle` - OpenRouter handle
- `enable_streaming` - `true` to enable streaming, `false` to disable

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_INVALID_ARG` if handle is NULL

**Example:**
```c
// Enable streaming for real-time responses
openrouter_set_streaming(handle, true);
```

---

## Basic API Calls

### openrouter_call

```c
esp_err_t openrouter_call(openrouter_handle_t handle, const char *prompt, 
                         char *response, size_t response_size);
```

Makes a synchronous API call and returns the complete response.

**Parameters:**
- `handle` - OpenRouter handle
- `prompt` - User prompt/question
- `response` - Buffer to store the response
- `response_size` - Size of the response buffer

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_INVALID_ARG` if any parameter is NULL
- `ESP_ERR_NO_MEM` if memory allocation fails
- `ESP_FAIL` on HTTP or API errors

**Example:**
```c
char response[2048];
esp_err_t err = openrouter_call(handle, 
    "Explain how ESP32 GPIO works", 
    response, sizeof(response));

if (err == ESP_OK) {
    ESP_LOGI(TAG, "AI Response: %s", response);
} else {
    ESP_LOGE(TAG, "API call failed: %s", esp_err_to_name(err));
}
```

---

### openrouter_call_with_system

```c
esp_err_t openrouter_call_with_system(openrouter_handle_t handle, 
                                     const char *system_role, const char *prompt,
                                     char *response, size_t response_size);
```

Makes an API call with a specific system role for this request only.

**Parameters:**
- `handle` - OpenRouter handle
- `system_role` - System role for this specific call
- `prompt` - User prompt/question
- `response` - Buffer to store the response
- `response_size` - Size of the response buffer

**Returns:**
- Same as `openrouter_call`

**Example:**
```c
char response[2048];
esp_err_t err = openrouter_call_with_system(handle,
    "You are an ESP32 programming expert",
    "How do I configure I2C on ESP32?",
    response, sizeof(response));
```

---

## Streaming API Calls

### openrouter_call_streaming

```c
esp_err_t openrouter_call_streaming(openrouter_handle_t handle, const char *prompt,
                                   openrouter_stream_callback_t callback, void *user_data);
```

Makes a streaming API call where responses are delivered token by token.

**Parameters:**
- `handle` - OpenRouter handle
- `prompt` - User prompt/question
- `callback` - Function called for each token
- `user_data` - User data passed to callback

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_INVALID_ARG` if any required parameter is NULL
- `ESP_FAIL` on HTTP or API errors

**Example:**
```c
void stream_callback(const char *content, bool is_complete, void *user_data) {
    if (is_complete) {
        printf("\n[Stream Complete]\n");
    } else {
        printf("%s", content);
        fflush(stdout);
    }
}

esp_err_t err = openrouter_call_streaming(handle,
    "Tell me about ESP32 features",
    stream_callback, NULL);
```

---

### Stream Callback Function

```c
typedef void (*openrouter_stream_callback_t)(const char *content, bool is_complete, void *user_data);
```

Callback function type for streaming responses.

**Parameters:**
- `content` - Token content (empty string when `is_complete` is true)
- `is_complete` - `true` when stream ends, `false` for intermediate tokens
- `user_data` - User data passed to the streaming call

**Notes:**
- Called multiple times during streaming
- Final call has `is_complete = true` and `content` is empty
- Content is only valid during the callback

---

## Function Calling (Tools)

### openrouter_register_function

```c
esp_err_t openrouter_register_function(openrouter_handle_t handle, 
                                      const openrouter_function_t *function);
```

Registers a function that can be called by the AI.

**Parameters:**
- `handle` - OpenRouter handle
- `function` - Function definition structure

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_INVALID_ARG` if parameters are invalid
- `ESP_ERR_OPENROUTER_FUNCTION_EXISTS` if function name already registered
- `ESP_ERR_OPENROUTER_MAX_FUNCTIONS` if maximum functions exceeded

**Example:**
```c
char* get_temperature(const char *function_name, const char *arguments, void *user_data) {
    // Parse arguments and get temperature
    return strdup("{\"temperature\": 25.5, \"unit\": \"celsius\"}");
}

openrouter_function_t temp_func = {
    .name = "get_temperature",
    .description = "Get current temperature from sensor",
    .parameters = temp_params_schema, // cJSON object
    .callback = get_temperature,
    .user_data = NULL
};

esp_err_t err = openrouter_register_function(handle, &temp_func);
```

---

### openrouter_register_simple_function

```c
esp_err_t openrouter_register_simple_function(openrouter_handle_t handle, 
                                             const openrouter_simple_function_t *function);
```

Registers a function using simplified parameter definitions (recommended).

**Parameters:**
- `handle` - OpenRouter handle
- `function` - Simplified function definition

**Returns:**
- Same as `openrouter_register_function`

**Example:**
```c
// Define parameters
static const openrouter_param_t temp_params[] = {
    {"sensor_id", "string", "ID of the temperature sensor", true, NULL},
    {"unit", "string", "Temperature unit", false, (const char*[]){"celsius", "fahrenheit", NULL}},
    {NULL, NULL, NULL, false, NULL} // Terminator
};

// Register function
openrouter_simple_function_t temp_func = {
    .name = "get_temperature",
    .description = "Get temperature from specified sensor",
    .parameters = temp_params,
    .callback = get_temperature,
    .user_data = NULL
};

esp_err_t err = openrouter_register_simple_function(handle, &temp_func);
```

---

### openrouter_call_with_tools

```c
esp_err_t openrouter_call_with_tools(openrouter_handle_t handle, const char *prompt,
                                    char *response, size_t response_size, int max_tool_iterations);
```

Makes an API call with automatic tool execution.

**Parameters:**
- `handle` - OpenRouter handle
- `prompt` - User prompt/question
- `response` - Buffer to store final response
- `response_size` - Size of response buffer
- `max_tool_iterations` - Maximum number of tool call rounds

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_OPENROUTER_TOOLS_DISABLED` if tools not enabled
- Other errors as per basic API calls

**Example:**
```c
char response[4096];
esp_err_t err = openrouter_call_with_tools(handle,
    "What's the current temperature?",
    response, sizeof(response), 5);

if (err == ESP_OK) {
    ESP_LOGI(TAG, "Final response: %s", response);
}
```

---

### Function Callback

```c
typedef char *(*openrouter_function_callback_t)(const char *function_name, 
                                               const char *arguments, void *user_data);
```

Callback function type for registered functions.

**Parameters:**
- `function_name` - Name of the function being called
- `arguments` - JSON string with function arguments
- `user_data` - User data provided during registration

**Returns:**
- `char*` - JSON string with result (must be freed by caller)
- `NULL` on error

**Example:**
```c
char* my_function(const char *function_name, const char *arguments, void *user_data) {
    // Parse arguments
    cJSON *args = cJSON_Parse(arguments);
    if (!args) return NULL;
    
    // Execute function logic
    int result = perform_calculation(args);
    cJSON_Delete(args);
    
    // Return JSON result
    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "result", result);
    char *json_string = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    
    return json_string; // Caller will free this
}
```

---

## Multimodal API Calls

### openrouter_call_with_image

```c
esp_err_t openrouter_call_with_image(openrouter_handle_t handle, const char *prompt,
                                    const char *image_path, char *response, size_t response_size);
```

Makes an API call with an image file for analysis.

**Parameters:**
- `handle` - OpenRouter handle
- `prompt` - Text prompt describing what to do with the image
- `image_path` - Path to image file (JPEG, PNG supported)
- `response` - Buffer to store response
- `response_size` - Size of response buffer

**Returns:**
- `ESP_OK` on success
- Standard error codes on failure

**Example:**
```c
char response[4096];
esp_err_t err = openrouter_call_with_image(handle,
    "Describe what you see in this image",
    "/spiffs/camera_capture.jpg",
    response, sizeof(response));
```

---

### openrouter_call_with_image_url

```c
esp_err_t openrouter_call_with_image_url(openrouter_handle_t handle, const char *prompt,
                                        const char *image_url, char *response, size_t response_size);
```

Makes an API call with an image from a URL.

**Parameters:**
- `handle` - OpenRouter handle
- `prompt` - Text prompt
- `image_url` - URL to image
- `response` - Buffer to store response
- `response_size` - Size of response buffer

**Example:**
```c
esp_err_t err = openrouter_call_with_image_url(handle,
    "What's the main subject of this image?",
    "https://example.com/image.jpg",
    response, sizeof(response));
```

---

### openrouter_call_with_audio

```c
esp_err_t openrouter_call_with_audio(openrouter_handle_t handle, const char *prompt,
                                    const char *audio_path, char *response, size_t response_size);
```

Makes an API call with an audio file for transcription or analysis.

**Parameters:**
- `handle` - OpenRouter handle
- `prompt` - Text prompt (e.g., "Transcribe this audio")
- `audio_path` - Path to audio file (MP3, WAV supported)
- `response` - Buffer to store response
- `response_size` - Size of response buffer

**Example:**
```c
char response[4096];
esp_err_t err = openrouter_call_with_audio(handle,
    "Transcribe this audio recording",
    "/spiffs/voice_memo.mp3",
    response, sizeof(response));
```

---

## Utility Functions

### openrouter_unregister_function

```c
esp_err_t openrouter_unregister_function(openrouter_handle_t handle, const char *function_name);
```

Unregisters a previously registered function.

**Parameters:**
- `handle` - OpenRouter handle
- `function_name` - Name of function to unregister

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_NOT_FOUND` if function not found

---

### openrouter_clear_functions

```c
esp_err_t openrouter_clear_functions(openrouter_handle_t handle);
```

Unregisters all functions.

**Parameters:**
- `handle` - OpenRouter handle

**Returns:**
- `ESP_OK` on success

---

### openrouter_set_tools_enabled

```c
esp_err_t openrouter_set_tools_enabled(openrouter_handle_t handle, bool enable_tools);
```

Enables or disables function calling.

**Parameters:**
- `handle` - OpenRouter handle
- `enable_tools` - `true` to enable, `false` to disable

**Returns:**
- `ESP_OK` on success

---

## Constants and Macros

### Default Values

```c
#define OPENROUTER_DEFAULT_RESPONSE_BUFFER_SIZE CONFIG_OPENROUTER_DEFAULT_RESPONSE_BUFFER_SIZE
#define OPENROUTER_DEFAULT_TEMPERATURE atof(CONFIG_OPENROUTER_DEFAULT_TEMPERATURE)
#define OPENROUTER_DEFAULT_MAX_TOKENS CONFIG_OPENROUTER_DEFAULT_MAX_TOKENS
#define OPENROUTER_DEFAULT_TOP_P atof(CONFIG_OPENROUTER_DEFAULT_TOP_P)
#define OPENROUTER_DEFAULT_HTTP_TIMEOUT CONFIG_OPENROUTER_DEFAULT_HTTP_TIMEOUT
```

### Error Codes

```c
#define ESP_ERR_OPENROUTER_MEMORY (ESP_ERR_OPENROUTER_BASE + 1)
#define ESP_ERR_OPENROUTER_JSON_PARSE (ESP_ERR_OPENROUTER_BASE + 2)
#define ESP_ERR_OPENROUTER_API_ERROR (ESP_ERR_OPENROUTER_BASE + 3)
#define ESP_ERR_OPENROUTER_HTTP_ERROR (ESP_ERR_OPENROUTER_BASE + 4)
#define ESP_ERR_OPENROUTER_FUNCTION_EXISTS (ESP_ERR_OPENROUTER_BASE + 5)
#define ESP_ERR_OPENROUTER_FUNCTION_NOT_FOUND (ESP_ERR_OPENROUTER_BASE + 6)
#define ESP_ERR_OPENROUTER_TOOLS_DISABLED (ESP_ERR_OPENROUTER_BASE + 7)
#define ESP_ERR_OPENROUTER_MAX_FUNCTIONS (ESP_ERR_OPENROUTER_BASE + 8)
```

### Limits

```c
#define MAX_API_KEY_LENGTH 256
#define MAX_MODEL_NAME_LENGTH 128
#define MAX_SYSTEM_ROLE_LENGTH 2048
#define MAX_REGISTERED_FUNCTIONS 32
#define MAX_IMAGE_FILE_SIZE (10 * 1024 * 1024)  // 10MB
#define MAX_AUDIO_FILE_SIZE (25 * 1024 * 1024)  // 25MB
```

---

## Thread Safety

All functions in this library are thread-safe and can be called concurrently from multiple tasks. The library uses internal mutexes to ensure data consistency.

## Memory Management

- All string parameters are copied internally
- Returned strings from callbacks must be freed by the caller
- Handles must be destroyed with `openrouter_destroy()`
- The library uses dynamic memory allocation with automatic buffer growth

---

*For more detailed usage examples, see the [examples directory](../examples/) and other documentation pages.*
