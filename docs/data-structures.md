# Data Structures

This document details all data structures, types, and enumerations used in the OpenRouter ESP-IDF library.

## Table of Contents

- [Configuration Structures](#configuration-structures)
- [Function Definition Structures](#function-definition-structures)
- [Parameter Definition Structures](#parameter-definition-structures)
- [Callback Types](#callback-types)
- [Handle Types](#handle-types)
- [Internal Structures](#internal-structures)

---

## Configuration Structures

### openrouter_config_t

Main configuration structure for creating an OpenRouter client handle.

```c
typedef struct {
    const char *api_key;                    // Required: OpenRouter API key
    const char *default_model;              // AI model to use (NULL for default)
    const char *default_system_role;        // System role/instructions (NULL for none)
    float temperature;                      // Response randomness (0.0-2.0, 0 for default)
    int max_tokens;                        // Maximum response tokens (0 for default)
    float top_p;                           // Top-p sampling parameter (0.0-1.0, 0 for default)
    int seed;                              // Random seed (-1 for random)
    size_t response_buffer_size;           // Internal buffer size (0 for default)
    bool enable_streaming;                 // Enable streaming mode
    bool enable_tools;                     // Enable function calling
    uint32_t http_timeout_ms;              // HTTP timeout (0 for default)
    bool enable_connection_reuse;          // Enable HTTP connection pooling
    uint32_t connection_timeout_ms;        // Connection keep-alive timeout (0 for default)
    const char *http_referer;              // HTTP-Referer header value (optional)
    const char *x_title;                   // X-Title header value (optional)
    openrouter_tool_callback_t tool_callback;  // Tool callback function (optional)
    void *tool_user_data;                  // User data for tool callback (optional)
} openrouter_config_t;
```

**Field Details:**

| Field | Type | Required | Description | Default |
|-------|------|----------|-------------|---------|
| `api_key` | `const char*` | ✅ | OpenRouter API key from openrouter.ai | - |
| `default_model` | `const char*` | ❌ | AI model identifier | `"openai/gpt-4o-mini"` |
| `default_system_role` | `const char*` | ❌ | System prompt defining AI behavior | `NULL` |
| `temperature` | `float` | ❌ | Creativity level (0.0=focused, 2.0=creative) | `0.7` |
| `max_tokens` | `int` | ❌ | Maximum tokens in response | `1024` |
| `top_p` | `float` | ❌ | Nucleus sampling parameter | `1.0` |
| `seed` | `int` | ❌ | Random seed for deterministic output | `-1` (random) |
| `response_buffer_size` | `size_t` | ❌ | Internal response buffer size | `4096` |
| `enable_streaming` | `bool` | ❌ | Enable real-time token streaming | `false` |
| `enable_tools` | `bool` | ❌ | Enable AI function calling | `false` |
| `http_timeout_ms` | `uint32_t` | ❌ | HTTP request timeout | `30000` |
| `enable_connection_reuse` | `bool` | ❌ | Enable HTTP connection pooling | `false` |
| `connection_timeout_ms` | `uint32_t` | ❌ | Keep-alive connection timeout | `30000` |
| `http_referer` | `const char*` | ❌ | HTTP Referer header | `NULL` |
| `x_title` | `const char*` | ❌ | X-Title header for tracking | `NULL` |
| `tool_callback` | `openrouter_tool_callback_t` | ❌ | Tool execution callback | `NULL` |
| `tool_user_data` | `void*` | ❌ | User data for tool callback | `NULL` |

**Example:**
```c
openrouter_config_t config = {
    .api_key = "sk-or-v1-abcd1234...",
    .default_model = "openai/gpt-4",
    .default_system_role = "You are a helpful IoT assistant.",
    .temperature = 0.7f,
    .max_tokens = 2048,
    .top_p = 0.9f,
    .seed = -1,
    .response_buffer_size = 8192,
    .enable_streaming = true,
    .enable_tools = true,
    .http_timeout_ms = 60000,
    .enable_connection_reuse = true,
    .connection_timeout_ms = 30000
};
```

---

## Function Definition Structures

### openrouter_function_t

Advanced function definition structure for registering AI-callable functions.

```c
typedef struct {
    const char *name;                        // Function name
    const char *description;                 // Function description
    const cJSON *parameters;                 // JSON schema for parameters
    openrouter_function_callback_t callback; // Function implementation
    void *user_data;                         // User data passed to callback
} openrouter_function_t;
```

**Field Details:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | `const char*` | ✅ | Function name (alphanumeric, underscore, hyphen only) |
| `description` | `const char*` | ✅ | Clear description of what the function does |
| `parameters` | `const cJSON*` | ❌ | JSON Schema for function parameters |
| `callback` | `openrouter_function_callback_t` | ✅ | Function implementation callback |
| `user_data` | `void*` | ❌ | User data passed to callback |

**Example:**
```c
// Create JSON schema for parameters
cJSON *schema = cJSON_CreateObject();
cJSON *properties = cJSON_CreateObject();
cJSON *temp_prop = cJSON_CreateObject();
cJSON_AddStringToObject(temp_prop, "type", "string");
cJSON_AddStringToObject(temp_prop, "description", "Temperature sensor ID");
cJSON_AddItemToObject(properties, "sensor_id", temp_prop);
cJSON_AddItemToObject(schema, "properties", properties);

openrouter_function_t func = {
    .name = "get_temperature",
    .description = "Get temperature reading from sensor",
    .parameters = schema,
    .callback = temperature_callback,
    .user_data = &sensor_data
};
```

---

### openrouter_simple_function_t

Simplified function definition structure (recommended for most use cases).

```c
typedef struct {
    const char *name;                        // Function name
    const char *description;                 // Function description
    const openrouter_param_t *parameters;   // Array of simplified parameters
    openrouter_function_callback_t callback; // Function implementation
    void *user_data;                         // User data passed to callback
} openrouter_simple_function_t;
```

**Field Details:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | `const char*` | ✅ | Function name |
| `description` | `const char*` | ✅ | Function description |
| `parameters` | `const openrouter_param_t*` | ❌ | Array of parameter definitions (NULL-terminated) |
| `callback` | `openrouter_function_callback_t` | ✅ | Function implementation |
| `user_data` | `void*` | ❌ | User data for callback |

**Example:**
```c
static const openrouter_param_t params[] = {
    {"sensor_id", "string", "ID of temperature sensor", true, NULL},
    {"unit", "string", "Temperature unit", false, (const char*[]){"celsius", "fahrenheit", NULL}},
    {NULL, NULL, NULL, false, NULL} // Terminator
};

openrouter_simple_function_t func = {
    .name = "get_temperature",
    .description = "Get temperature from specified sensor",
    .parameters = params,
    .callback = temperature_callback,
    .user_data = NULL
};
```

---

## Parameter Definition Structures

### openrouter_param_t

Simplified parameter definition for function parameters.

```c
typedef struct {
    const char *name;         // Parameter name
    const char *type;         // Parameter type
    const char *description;  // Parameter description
    bool required;            // Whether parameter is required
    const char **enum_values; // Array of allowed values (NULL-terminated, NULL if not enum)
} openrouter_param_t;
```

**Field Details:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | `const char*` | ✅ | Parameter name |
| `type` | `const char*` | ✅ | JSON Schema type: `"string"`, `"number"`, `"boolean"`, `"array"`, `"object"` |
| `description` | `const char*` | ✅ | Clear description of the parameter |
| `required` | `bool` | ✅ | Whether this parameter is required |
| `enum_values` | `const char**` | ❌ | NULL-terminated array of allowed string values (NULL if not enum) |

**Supported Types:**
- `"string"` - Text values
- `"number"` - Numeric values (integer or float)
- `"boolean"` - True/false values
- `"array"` - Array of values
- `"object"` - JSON object

**Example:**
```c
// String parameter with enum values
static const char* units[] = {"celsius", "fahrenheit", "kelvin", NULL};

// Parameter definitions
static const openrouter_param_t weather_params[] = {
    {
        .name = "location",
        .type = "string",
        .description = "City and state, e.g. San Francisco, CA",
        .required = true,
        .enum_values = NULL
    },
    {
        .name = "unit",
        .type = "string", 
        .description = "Temperature unit to use",
        .required = false,
        .enum_values = units
    },
    {
        .name = "include_forecast",
        .type = "boolean",
        .description = "Whether to include weather forecast",
        .required = false,
        .enum_values = NULL
    },
    {NULL, NULL, NULL, false, NULL} // Terminator
};
```

---

## Callback Types

### openrouter_stream_callback_t

Callback function type for streaming responses.

```c
typedef void (*openrouter_stream_callback_t)(const char *content, bool is_complete, void *user_data);
```

**Parameters:**
- `content` - Token content (empty string when `is_complete` is true)
- `is_complete` - `true` when stream ends, `false` for intermediate tokens
- `user_data` - User data passed to the streaming call

**Example:**
```c
void my_stream_callback(const char *content, bool is_complete, void *user_data) {
    if (is_complete) {
        printf("\n[Stream finished]\n");
        // Optional: signal completion to user_data
        if (user_data) {
            bool *finished = (bool*)user_data;
            *finished = true;
        }
    } else {
        printf("%s", content);
        fflush(stdout);
    }
}

// Usage
bool stream_finished = false;
openrouter_call_streaming(handle, "Tell me a story", my_stream_callback, &stream_finished);
```

---

### openrouter_function_callback_t

Callback function type for registered functions.

```c
typedef char *(*openrouter_function_callback_t)(const char *function_name, 
                                               const char *arguments, void *user_data);
```

**Parameters:**
- `function_name` - Name of the function being called
- `arguments` - JSON string containing function arguments
- `user_data` - User data provided during function registration

**Returns:**
- `char*` - JSON string containing function result (must be freed by caller)
- `NULL` - On error

**Example:**
```c
char* get_sensor_data(const char *function_name, const char *arguments, void *user_data) {
    // Parse arguments
    cJSON *args = cJSON_Parse(arguments);
    if (!args) {
        return strdup("{\"error\": \"Invalid JSON arguments\"}");
    }
    
    cJSON *sensor_id_item = cJSON_GetObjectItem(args, "sensor_id");
    if (!sensor_id_item || !cJSON_IsString(sensor_id_item)) {
        cJSON_Delete(args);
        return strdup("{\"error\": \"Missing or invalid sensor_id\"}");
    }
    
    const char *sensor_id = sensor_id_item->valuestring;
    
    // Read sensor (example)
    float temperature = read_temperature_sensor(sensor_id);
    
    // Create response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "sensor_id", sensor_id);
    cJSON_AddNumberToObject(response, "temperature", temperature);
    cJSON_AddStringToObject(response, "unit", "celsius");
    cJSON_AddStringToObject(response, "timestamp", get_current_timestamp());
    
    char *result = cJSON_PrintUnformatted(response);
    
    // Cleanup
    cJSON_Delete(args);
    cJSON_Delete(response);
    
    return result; // Caller will free this
}
```

---

### openrouter_tool_callback_t

Alternative callback type for tool calls (provides tool call ID).

```c
typedef char *(*openrouter_tool_callback_t)(const char *tool_call_id, 
                                           const char *function_name,
                                           const char *arguments, void *user_data);
```

**Parameters:**
- `tool_call_id` - Unique identifier for this tool call
- `function_name` - Name of the function being called
- `arguments` - JSON string containing function arguments
- `user_data` - User data provided during configuration

**Returns:**
- Same as `openrouter_function_callback_t`

---

## Handle Types

### openrouter_handle_t

Opaque handle representing an OpenRouter client instance.

```c
typedef struct openrouter_handle *openrouter_handle_t;
```

**Notes:**
- Created with `openrouter_create()`
- Must be destroyed with `openrouter_destroy()`
- Thread-safe for concurrent usage
- Contains all client state and configuration

---

## Internal Structures

These structures are used internally by the library and are provided here for completeness. They should not be accessed directly by user code.

### response_data_t

Internal structure for non-streaming response data.

```c
typedef struct {
    char *buffer;     // Response buffer
    size_t size;      // Total buffer size
    size_t pos;       // Current position in buffer
} response_data_t;
```

---

### streaming_response_data_t

Internal structure for streaming response data.

```c
typedef struct {
    char *buffer;                          // Buffer for full response accumulation
    size_t size;                           // Total buffer size
    size_t pos;                            // Current position in buffer
    openrouter_stream_callback_t callback; // User callback function
    void *user_data;                       // User data for callback
    char *line_buffer;                     // Buffer for incomplete lines
    size_t line_buffer_size;               // Line buffer size
    size_t line_buffer_pos;                // Current position in line buffer
    bool done_sent;                        // Track if completion callback was sent
    char *accumulated_data;                // Buffer for multi-line data events
    size_t accumulated_data_size;          // Size of accumulated data buffer
    size_t accumulated_data_pos;           // Position in accumulated data buffer
} streaming_response_data_t;
```

---

### config_snapshot_t

Internal structure for thread-safe configuration snapshots.

```c
typedef struct {
    char *api_key;                            // API key copy
    char *model;                              // Model name copy
    char *system_role;                        // System role copy
    float temperature;                        // Temperature parameter
    int max_tokens;                           // Maximum tokens
    float top_p;                              // Top_p parameter
    int seed;                                 // Random seed
    bool enable_streaming;                    // Streaming mode flag
    uint32_t http_timeout_ms;                 // HTTP timeout
    char *http_referer;                       // HTTP-Referer header copy
    char *x_title;                            // X-Title header copy
    bool enable_tools;                        // Tools enabled flag
    openrouter_tool_callback_t tool_callback; // Tool callback copy
    void *tool_user_data;                     // Tool user data copy
    cJSON *tools_array;                       // Tools array for API
    int max_retry_attempts;                   // Maximum retry attempts
    uint32_t retry_delay_ms;                  // Base retry delay
    uint32_t max_retry_delay_ms;              // Maximum retry delay
} config_snapshot_t;
```

---

## Memory Management Notes

### String Handling
- All string fields in configuration structures are copied internally
- Returned strings from callbacks must be freed by the caller using `free()`
- The library handles all internal string memory management

### JSON Objects
- JSON objects passed to the library are copied when needed
- User code is responsible for freeing JSON objects it creates
- The library creates and manages its own JSON objects internally

### Buffer Management
- Response buffers are managed automatically by the library
- Buffer sizes grow dynamically as needed
- Maximum buffer sizes are enforced to prevent memory exhaustion

---

## Thread Safety

All structures and handles in this library are designed to be thread-safe:

- Multiple threads can safely use the same handle concurrently
- Internal mutexes protect shared state
- Configuration snapshots ensure consistent parameter sets during API calls
- Callback functions should be reentrant if called from multiple threads

---

*For usage examples of these structures, see the [API Reference](api-reference.md) and [examples directory](../examples/).*
