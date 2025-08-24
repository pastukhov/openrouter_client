# OpenRouter ESP-IDF Function Calling Example

This example demonstrates how to use the OpenRouter ESP-IDF library with function calling (tools) capabilities using the simplified API.

## Features

- Register custom functions that the AI can call using simple parameter definitions
- Weather information function (mock implementation)
- Mathematical calculation function
- LED control function
- Automatic tool execution and conversation continuation
- No direct cJSON usage - all complexity is handled internally

## Setup

1. Set your WiFi credentials in `main/main.c`:
   ```c
   #define WIFI_SSID "YOUR_WIFI_SSID"
   #define WIFI_PASS "YOUR_WIFI_PASSWORD"
   ```

2. Set your OpenRouter API key in `main/main.c`:
   ```c
   #define OPENROUTER_API_KEY "YOUR_OPENROUTER_API_KEY"
   ```

3. Build and flash:
   ```bash
   idf.py build
   idf.py flash monitor
   ```

## How it Works

1. **Simple Function Registration**: The example registers three functions using the simplified API:
   - `get_current_weather`: Returns mock weather data for a location
   - `calculate_math`: Performs basic mathematical calculations
   - `control_led`: Controls LED state and color

2. **Easy Parameter Definition**: Each function uses simple parameter structures:
   ```c
   static const openrouter_param_t weather_params[] = {
       {"location", "string", "The city and state, e.g. San Francisco, CA", true, NULL},
       {"unit", "string", "Temperature unit", false, temp_units},
       {NULL, NULL, NULL, false, NULL}  // Terminator
   };
   ```

3. **Automatic Schema Generation**: The library automatically creates JSON schemas from the simple parameter definitions.

4. **Automatic Execution**: When the AI wants to call a function, the library automatically:
   - Executes the registered callback
   - Adds the result to the conversation
   - Continues the conversation with the AI

5. **Examples Tested**:
   - Weather query: "What's the weather like in New York?"
   - Math calculation: "What is 25 * 4?"
   - LED control: "Turn on the LED with red color"
   - Combined query: Multiple function calls in one request
   - Regular conversation: "Tell me a short joke about programming."

## Simple Function Implementation

Functions must:
- Accept three parameters: `function_name`, `arguments` (JSON string), `user_data`
- Return a JSON string result (caller must free)
- Handle errors gracefully

Example function:
```c
char* get_current_weather(const char *function_name, const char *arguments, void *user_data)
{
    ESP_LOGI(TAG, "Getting weather for location from arguments: %s", arguments);
    
    // In a real implementation:
    // 1. Parse the JSON arguments to extract location and unit
    // 2. Make API call to weather service or read sensors
    // 3. Format response as JSON
    
    // For this example, return mock weather data
    return strdup("{\"temperature\": 22.5, \"unit\": \"celsius\", \"description\": \"Partly cloudy\"}");
}
```

## Parameter Types

Supported parameter types:
- `"string"`: Text values
- `"number"`: Numeric values
- `"boolean"`: True/false values
- `"array"`: List of values
- `"object"`: Nested objects

## Parameter Features

- **Required/Optional**: Set the `required` field to `true` or `false`
- **Enums**: Provide a NULL-terminated array of allowed string values
- **Descriptions**: Help the AI understand what each parameter does

Example with enum:
```c
static const char* led_states[] = {"on", "off", NULL};
static const openrouter_param_t led_params[] = {
    {"state", "string", "LED state", true, led_states},
    {NULL, NULL, NULL, false, NULL}
};
```

## Configuration

The library supports various configuration options:
- `enable_tools`: Enable function calling
- `max_tool_iterations`: Limit recursive function calls
- Function registration: Add/remove functions dynamically

## Notes

- Functions are executed synchronously in the main task
- Function results are automatically added to the conversation
- The AI can call multiple functions in sequence
- Set reasonable limits on tool iterations to prevent infinite loops
- Always validate function arguments and handle errors
- The simplified API handles all JSON schema complexity internally
