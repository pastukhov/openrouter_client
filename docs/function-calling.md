# Function Calling Guide

This guide covers how to use function calling (tools) with the OpenRouter ESP-IDF library, enabling AI models to call custom functions during conversations.

## Table of Contents

- [What is Function Calling?](#what-is-function-calling)
- [Function Registry System](#function-registry-system)
- [Defining Functions](#defining-functions)
- [Function Parameters](#function-parameters)
- [Advanced Function Patterns](#advanced-function-patterns)
- [Error Handling](#error-handling)
- [Performance Considerations](#performance-considerations)
- [Best Practices](#best-practices)
- [Real-World Examples](#real-world-examples)

---

## What is Function Calling?

Function calling allows AI models to execute custom functions during conversation, enabling:

- **Dynamic Data Access**: Read sensors, databases, files
- **External API Integration**: Weather, IoT services, web APIs
- **Device Control**: GPIO operations, motor control, display updates
- **Complex Logic**: Mathematical calculations, data processing

### Function Calling Flow

```
User Query → AI Model → Function Call → Your Code → Result → AI Response
```

Example:
```
User: "What's the temperature?"
AI: Calls get_temperature()
Your Function: Returns "23.5°C"
AI: "The current temperature is 23.5°C"
```

---

## Function Registry System

### 1. Basic Function Registration

```c
// Function implementation
esp_err_t get_temperature(cJSON *params, cJSON **result, void *user_data) {
    // Read temperature from sensor
    float temp = read_temperature_sensor();
    
    // Create JSON result
    *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(*result, "temperature", temp);
    cJSON_AddStringToObject(*result, "unit", "celsius");
    
    return ESP_OK;
}

// Register the function
esp_err_t setup_functions(openrouter_handle_t handle) {
    // Function description for AI
    cJSON *func_desc = cJSON_CreateObject();
    cJSON_AddStringToObject(func_desc, "name", "get_temperature");
    cJSON_AddStringToObject(func_desc, "description", "Get current temperature from sensor");
    
    // No parameters needed for this function
    cJSON *params_schema = cJSON_CreateObject();
    cJSON_AddStringToObject(params_schema, "type", "object");
    cJSON_AddObjectToObject(func_desc, "parameters", params_schema);
    
    // Register function
    esp_err_t err = openrouter_register_function(handle, func_desc, get_temperature, NULL);
    cJSON_Delete(func_desc);
    
    return err;
}
```

### 2. Function with Parameters

```c
// Function that takes parameters
esp_err_t set_led_color(cJSON *params, cJSON **result, void *user_data) {
    // Extract parameters
    cJSON *red_json = cJSON_GetObjectItem(params, "red");
    cJSON *green_json = cJSON_GetObjectItem(params, "green");
    cJSON *blue_json = cJSON_GetObjectItem(params, "blue");
    
    if (!cJSON_IsNumber(red_json) || !cJSON_IsNumber(green_json) || !cJSON_IsNumber(blue_json)) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "Invalid color parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    int red = red_json->valueint;
    int green = green_json->valueint;
    int blue = blue_json->valueint;
    
    // Validate ranges
    if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "Color values must be 0-255");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Set LED color (implementation specific)
    esp_err_t err = led_set_rgb(red, green, blue);
    
    // Create result
    *result = cJSON_CreateObject();
    if (err == ESP_OK) {
        cJSON_AddStringToObject(*result, "status", "success");
        cJSON_AddNumberToObject(*result, "red", red);
        cJSON_AddNumberToObject(*result, "green", green);
        cJSON_AddNumberToObject(*result, "blue", blue);
    } else {
        cJSON_AddStringToObject(*result, "error", "Failed to set LED color");
    }
    
    return err;
}

// Register function with parameter schema
esp_err_t register_led_function(openrouter_handle_t handle) {
    cJSON *func_desc = cJSON_CreateObject();
    cJSON_AddStringToObject(func_desc, "name", "set_led_color");
    cJSON_AddStringToObject(func_desc, "description", "Set RGB LED color");
    
    // Parameter schema
    cJSON *params_schema = cJSON_CreateObject();
    cJSON_AddStringToObject(params_schema, "type", "object");
    
    cJSON *properties = cJSON_CreateObject();
    
    // Red parameter
    cJSON *red_param = cJSON_CreateObject();
    cJSON_AddStringToObject(red_param, "type", "integer");
    cJSON_AddStringToObject(red_param, "description", "Red component (0-255)");
    cJSON_AddNumberToObject(red_param, "minimum", 0);
    cJSON_AddNumberToObject(red_param, "maximum", 255);
    cJSON_AddItemToObject(properties, "red", red_param);
    
    // Green parameter
    cJSON *green_param = cJSON_CreateObject();
    cJSON_AddStringToObject(green_param, "type", "integer");
    cJSON_AddStringToObject(green_param, "description", "Green component (0-255)");
    cJSON_AddNumberToObject(green_param, "minimum", 0);
    cJSON_AddNumberToObject(green_param, "maximum", 255);
    cJSON_AddItemToObject(properties, "green", green_param);
    
    // Blue parameter
    cJSON *blue_param = cJSON_CreateObject();
    cJSON_AddStringToObject(blue_param, "type", "integer");
    cJSON_AddStringToObject(blue_param, "description", "Blue component (0-255)");
    cJSON_AddNumberToObject(blue_param, "minimum", 0);
    cJSON_AddNumberToObject(blue_param, "maximum", 255);
    cJSON_AddItemToObject(properties, "blue", blue_param);
    
    cJSON_AddItemToObject(params_schema, "properties", properties);
    
    // Required parameters
    cJSON *required = cJSON_CreateArray();
    cJSON_AddItemToArray(required, cJSON_CreateString("red"));
    cJSON_AddItemToArray(required, cJSON_CreateString("green"));
    cJSON_AddItemToArray(required, cJSON_CreateString("blue"));
    cJSON_AddItemToObject(params_schema, "required", required);
    
    cJSON_AddItemToObject(func_desc, "parameters", params_schema);
    
    esp_err_t err = openrouter_register_function(handle, func_desc, set_led_color, NULL);
    cJSON_Delete(func_desc);
    
    return err;
}
```

---

## Defining Functions

### Function Signature

```c
typedef esp_err_t (*openrouter_function_t)(cJSON *params, cJSON **result, void *user_data);
```

**Parameters:**
- `params` - JSON object containing function parameters from AI
- `result` - Pointer to JSON object to return (you must create this)
- `user_data` - User context data passed during registration

**Return Value:**
- `ESP_OK` - Function executed successfully
- Other `esp_err_t` codes - Function failed (error will be reported to AI)

### Simple Function Example

```c
esp_err_t get_uptime(cJSON *params, cJSON **result, void *user_data) {
    uint32_t uptime_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t uptime_seconds = uptime_ms / 1000;
    
    *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(*result, "uptime_seconds", uptime_seconds);
    cJSON_AddNumberToObject(*result, "uptime_ms", uptime_ms);
    
    char uptime_str[64];
    uint32_t hours = uptime_seconds / 3600;
    uint32_t minutes = (uptime_seconds % 3600) / 60;
    uint32_t seconds = uptime_seconds % 60;
    snprintf(uptime_str, sizeof(uptime_str), "%02luh:%02lum:%02lus", hours, minutes, seconds);
    cJSON_AddStringToObject(*result, "formatted", uptime_str);
    
    return ESP_OK;
}
```

### Function with Error Handling

```c
esp_err_t read_file(cJSON *params, cJSON **result, void *user_data) {
    // Extract filename parameter
    cJSON *filename_json = cJSON_GetObjectItem(params, "filename");
    if (!cJSON_IsString(filename_json)) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "filename parameter is required");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *filename = filename_json->valuestring;
    
    // Validate filename (security check)
    if (strstr(filename, "..") || filename[0] == '/') {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "Invalid filename");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Open and read file
    FILE *f = fopen(filename, "r");
    if (!f) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "File not found");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size > 4096) {  // Limit file size
        fclose(f);
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "File too large");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Read file content
    char *content = malloc(file_size + 1);
    if (!content) {
        fclose(f);
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "Out of memory");
        return ESP_ERR_NO_MEM;
    }
    
    size_t read_size = fread(content, 1, file_size, f);
    content[read_size] = '\0';
    fclose(f);
    
    // Create result
    *result = cJSON_CreateObject();
    cJSON_AddStringToObject(*result, "filename", filename);
    cJSON_AddNumberToObject(*result, "size", read_size);
    cJSON_AddStringToObject(*result, "content", content);
    
    free(content);
    return ESP_OK;
}
```

---

## Function Parameters

### Parameter Types and Validation

```c
esp_err_t validate_and_extract_params(cJSON *params, 
                                     const char **string_param,
                                     int *int_param,
                                     double *double_param,
                                     bool *bool_param) {
    // String parameter
    cJSON *str_json = cJSON_GetObjectItem(params, "text");
    if (cJSON_IsString(str_json)) {
        *string_param = str_json->valuestring;
    } else {
        *string_param = NULL;
    }
    
    // Integer parameter
    cJSON *int_json = cJSON_GetObjectItem(params, "count");
    if (cJSON_IsNumber(int_json)) {
        *int_param = int_json->valueint;
    } else {
        *int_param = 0;
    }
    
    // Double parameter
    cJSON *double_json = cJSON_GetObjectItem(params, "value");
    if (cJSON_IsNumber(double_json)) {
        *double_param = double_json->valuedouble;
    } else {
        *double_param = 0.0;
    }
    
    // Boolean parameter
    cJSON *bool_json = cJSON_GetObjectItem(params, "enabled");
    if (cJSON_IsBool(bool_json)) {
        *bool_param = cJSON_IsTrue(bool_json);
    } else {
        *bool_param = false;
    }
    
    return ESP_OK;
}
```

### Complex Parameter Schemas

```c
// Function that takes array and object parameters
esp_err_t process_sensor_data(cJSON *params, cJSON **result, void *user_data) {
    // Extract array parameter
    cJSON *sensors_array = cJSON_GetObjectItem(params, "sensors");
    if (!cJSON_IsArray(sensors_array)) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "sensors must be an array");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Extract configuration object
    cJSON *config_obj = cJSON_GetObjectItem(params, "config");
    if (!cJSON_IsObject(config_obj)) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "config must be an object");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Process each sensor
    cJSON *results_array = cJSON_CreateArray();
    cJSON *sensor = NULL;
    cJSON_ArrayForEach(sensor, sensors_array) {
        if (cJSON_IsString(sensor)) {
            const char *sensor_name = sensor->valuestring;
            
            // Read sensor (implementation specific)
            float value = read_sensor(sensor_name);
            
            cJSON *sensor_result = cJSON_CreateObject();
            cJSON_AddStringToObject(sensor_result, "name", sensor_name);
            cJSON_AddNumberToObject(sensor_result, "value", value);
            cJSON_AddItemToArray(results_array, sensor_result);
        }
    }
    
    // Create result
    *result = cJSON_CreateObject();
    cJSON_AddItemToObject(*result, "sensor_readings", results_array);
    
    return ESP_OK;
}

// Register function with complex schema
esp_err_t register_sensor_function(openrouter_handle_t handle) {
    cJSON *func_desc = cJSON_CreateObject();
    cJSON_AddStringToObject(func_desc, "name", "process_sensor_data");
    cJSON_AddStringToObject(func_desc, "description", "Read data from multiple sensors");
    
    cJSON *params_schema = cJSON_CreateObject();
    cJSON_AddStringToObject(params_schema, "type", "object");
    
    cJSON *properties = cJSON_CreateObject();
    
    // Sensors array parameter
    cJSON *sensors_param = cJSON_CreateObject();
    cJSON_AddStringToObject(sensors_param, "type", "array");
    cJSON_AddStringToObject(sensors_param, "description", "List of sensor names to read");
    
    cJSON *items_schema = cJSON_CreateObject();
    cJSON_AddStringToObject(items_schema, "type", "string");
    cJSON_AddItemToObject(sensors_param, "items", items_schema);
    
    cJSON_AddItemToObject(properties, "sensors", sensors_param);
    
    // Config object parameter
    cJSON *config_param = cJSON_CreateObject();
    cJSON_AddStringToObject(config_param, "type", "object");
    cJSON_AddStringToObject(config_param, "description", "Configuration options");
    
    cJSON *config_props = cJSON_CreateObject();
    
    cJSON *interval_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(interval_prop, "type", "integer");
    cJSON_AddStringToObject(interval_prop, "description", "Sample interval in ms");
    cJSON_AddItemToObject(config_props, "interval", interval_prop);
    
    cJSON_AddItemToObject(config_param, "properties", config_props);
    cJSON_AddItemToObject(properties, "config", config_param);
    
    cJSON_AddItemToObject(params_schema, "properties", properties);
    
    // Required parameters
    cJSON *required = cJSON_CreateArray();
    cJSON_AddItemToArray(required, cJSON_CreateString("sensors"));
    cJSON_AddItemToObject(params_schema, "required", required);
    
    cJSON_AddItemToObject(func_desc, "parameters", params_schema);
    
    esp_err_t err = openrouter_register_function(handle, func_desc, process_sensor_data, NULL);
    cJSON_Delete(func_desc);
    
    return err;
}
```

---

## Advanced Function Patterns

### 1. Stateful Functions with User Data

```c
typedef struct {
    int counter;
    char last_message[256];
    uint32_t last_call_time;
} function_state_t;

esp_err_t stateful_function(cJSON *params, cJSON **result, void *user_data) {
    function_state_t *state = (function_state_t*)user_data;
    
    // Update state
    state->counter++;
    state->last_call_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Get message parameter
    cJSON *message_json = cJSON_GetObjectItem(params, "message");
    if (cJSON_IsString(message_json)) {
        strncpy(state->last_message, message_json->valuestring, sizeof(state->last_message) - 1);
        state->last_message[sizeof(state->last_message) - 1] = '\0';
    }
    
    // Create result with state information
    *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(*result, "call_count", state->counter);
    cJSON_AddStringToObject(*result, "last_message", state->last_message);
    cJSON_AddNumberToObject(*result, "last_call_time", state->last_call_time);
    
    return ESP_OK;
}

// Setup with state
esp_err_t setup_stateful_function(openrouter_handle_t handle) {
    static function_state_t state = {0};  // Static to persist
    
    cJSON *func_desc = cJSON_CreateObject();
    cJSON_AddStringToObject(func_desc, "name", "stateful_function");
    cJSON_AddStringToObject(func_desc, "description", "Function that maintains state between calls");
    
    // Parameter schema
    cJSON *params_schema = cJSON_CreateObject();
    cJSON_AddStringToObject(params_schema, "type", "object");
    
    cJSON *properties = cJSON_CreateObject();
    cJSON *message_param = cJSON_CreateObject();
    cJSON_AddStringToObject(message_param, "type", "string");
    cJSON_AddStringToObject(message_param, "description", "Message to store");
    cJSON_AddItemToObject(properties, "message", message_param);
    cJSON_AddItemToObject(params_schema, "properties", properties);
    cJSON_AddItemToObject(func_desc, "parameters", params_schema);
    
    // Register with state as user data
    esp_err_t err = openrouter_register_function(handle, func_desc, stateful_function, &state);
    cJSON_Delete(func_desc);
    
    return err;
}
```

### 2. Async Functions with Callbacks

```c
typedef struct {
    TaskHandle_t waiting_task;
    cJSON *result;
    bool completed;
    esp_err_t status;
} async_context_t;

// Task for async work
void async_worker_task(void *pvParameters) {
    async_context_t *ctx = (async_context_t*)pvParameters;
    
    // Simulate long-running operation
    vTaskDelay(pdMS_TO_TICKS(5000));  // 5 second delay
    
    // Create result
    ctx->result = cJSON_CreateObject();
    cJSON_AddStringToObject(ctx->result, "status", "completed");
    cJSON_AddStringToObject(ctx->result, "message", "Async operation finished");
    cJSON_AddNumberToObject(ctx->result, "duration_ms", 5000);
    
    ctx->completed = true;
    ctx->status = ESP_OK;
    
    // Notify waiting task
    if (ctx->waiting_task) {
        xTaskNotifyGive(ctx->waiting_task);
    }
    
    vTaskDelete(NULL);
}

esp_err_t async_function(cJSON *params, cJSON **result, void *user_data) {
    async_context_t ctx = {
        .waiting_task = xTaskGetCurrentTaskHandle(),
        .result = NULL,
        .completed = false,
        .status = ESP_FAIL
    };
    
    // Start async task
    BaseType_t task_created = xTaskCreate(async_worker_task, "async_worker", 
                                         4096, &ctx, 5, NULL);
    
    if (task_created != pdPASS) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "Failed to start async task");
        return ESP_ERR_NO_MEM;
    }
    
    // Wait for completion (with timeout)
    uint32_t notification_value = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10000));
    
    if (notification_value == 0) {
        // Timeout
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "Async operation timed out");
        return ESP_ERR_TIMEOUT;
    }
    
    // Return result
    *result = ctx.result;
    return ctx.status;
}
```

### 3. Function Chaining

```c
typedef struct {
    openrouter_handle_t handle;
    cJSON *chain_state;
} chain_context_t;

esp_err_t chain_step_1(cJSON *params, cJSON **result, void *user_data) {
    chain_context_t *ctx = (chain_context_t*)user_data;
    
    // Get input from params
    cJSON *input_json = cJSON_GetObjectItem(params, "input");
    const char *input = cJSON_IsString(input_json) ? input_json->valuestring : "";
    
    // Process step 1
    char step1_result[256];
    snprintf(step1_result, sizeof(step1_result), "Step1: Processed '%s'", input);
    
    // Store in chain state
    if (!ctx->chain_state) {
        ctx->chain_state = cJSON_CreateObject();
    }
    cJSON_AddStringToObject(ctx->chain_state, "step1_result", step1_result);
    
    // Return result
    *result = cJSON_CreateObject();
    cJSON_AddStringToObject(*result, "result", step1_result);
    cJSON_AddStringToObject(*result, "next_step", "Call chain_step_2 with this result");
    
    return ESP_OK;
}

esp_err_t chain_step_2(cJSON *params, cJSON **result, void *user_data) {
    chain_context_t *ctx = (chain_context_t*)user_data;
    
    // Get result from step 1
    cJSON *step1_json = cJSON_GetObjectItem(ctx->chain_state, "step1_result");
    const char *step1_result = cJSON_IsString(step1_json) ? step1_json->valuestring : "";
    
    // Get input from params
    cJSON *input_json = cJSON_GetObjectItem(params, "step1_output");
    const char *input = cJSON_IsString(input_json) ? input_json->valuestring : "";
    
    // Process step 2
    char final_result[512];
    snprintf(final_result, sizeof(final_result), 
             "Step2: Combined '%s' with '%s'", step1_result, input);
    
    // Clean up chain state
    if (ctx->chain_state) {
        cJSON_Delete(ctx->chain_state);
        ctx->chain_state = NULL;
    }
    
    // Return final result
    *result = cJSON_CreateObject();
    cJSON_AddStringToObject(*result, "final_result", final_result);
    cJSON_AddStringToObject(*result, "chain_complete", "true");
    
    return ESP_OK;
}
```

---

## Error Handling

### Comprehensive Error Handling Pattern

```c
esp_err_t robust_function(cJSON *params, cJSON **result, void *user_data) {
    esp_err_t ret = ESP_OK;
    *result = NULL;
    
    // Input validation
    if (!params) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "No parameters provided");
        cJSON_AddNumberToObject(*result, "error_code", ESP_ERR_INVALID_ARG);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Parameter extraction with validation
    cJSON *operation_json = cJSON_GetObjectItem(params, "operation");
    if (!cJSON_IsString(operation_json)) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "operation parameter is required");
        cJSON_AddNumberToObject(*result, "error_code", ESP_ERR_INVALID_ARG);
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *operation = operation_json->valuestring;
    
    // Create result object early for error reporting
    *result = cJSON_CreateObject();
    if (!*result) {
        ESP_LOGE(TAG, "Failed to create result JSON object");
        return ESP_ERR_NO_MEM;
    }
    
    // Operation-specific logic with error handling
    if (strcmp(operation, "read_sensor") == 0) {
        float sensor_value;
        ret = read_sensor_safe(&sensor_value);
        
        if (ret == ESP_OK) {
            cJSON_AddStringToObject(*result, "operation", "read_sensor");
            cJSON_AddNumberToObject(*result, "value", sensor_value);
            cJSON_AddStringToObject(*result, "status", "success");
        } else {
            cJSON_AddStringToObject(*result, "error", "Sensor read failed");
            cJSON_AddStringToObject(*result, "operation", "read_sensor");
            cJSON_AddNumberToObject(*result, "error_code", ret);
        }
        
    } else if (strcmp(operation, "write_config") == 0) {
        cJSON *config_json = cJSON_GetObjectItem(params, "config");
        if (!cJSON_IsObject(config_json)) {
            cJSON_AddStringToObject(*result, "error", "config parameter required for write_config");
            cJSON_AddNumberToObject(*result, "error_code", ESP_ERR_INVALID_ARG);
            ret = ESP_ERR_INVALID_ARG;
        } else {
            ret = write_config_safe(config_json);
            if (ret == ESP_OK) {
                cJSON_AddStringToObject(*result, "status", "success");
                cJSON_AddStringToObject(*result, "operation", "write_config");
            } else {
                cJSON_AddStringToObject(*result, "error", "Config write failed");
                cJSON_AddStringToObject(*result, "operation", "write_config");
                cJSON_AddNumberToObject(*result, "error_code", ret);
            }
        }
        
    } else {
        cJSON_AddStringToObject(*result, "error", "Unknown operation");
        cJSON_AddStringToObject(*result, "operation", operation);
        cJSON_AddNumberToObject(*result, "error_code", ESP_ERR_NOT_SUPPORTED);
        ret = ESP_ERR_NOT_SUPPORTED;
    }
    
    // Add timestamp and diagnostic info
    cJSON_AddNumberToObject(*result, "timestamp", xTaskGetTickCount() * portTICK_PERIOD_MS);
    cJSON_AddNumberToObject(*result, "free_heap", esp_get_free_heap_size());
    
    return ret;
}

// Safe sensor reading with error handling
esp_err_t read_sensor_safe(float *value) {
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Simulate sensor reading with potential failure
    static int failure_count = 0;
    
    if (failure_count++ % 10 == 0) {  // Simulate intermittent failure
        ESP_LOGW(TAG, "Simulated sensor failure");
        return ESP_ERR_TIMEOUT;
    }
    
    *value = 23.5f + (esp_random() % 100) / 10.0f;  // Random temperature
    return ESP_OK;
}

// Safe config writing with validation
esp_err_t write_config_safe(cJSON *config) {
    if (!config || !cJSON_IsObject(config)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate config structure
    cJSON *required_fields[] = {"device_id", "interval", "enabled"};
    for (int i = 0; i < sizeof(required_fields) / sizeof(required_fields[0]); i++) {
        if (!cJSON_GetObjectItem(config, required_fields[i])) {
            ESP_LOGE(TAG, "Missing required config field: %s", required_fields[i]);
            return ESP_ERR_INVALID_ARG;
        }
    }
    
    // Simulate config write
    ESP_LOGI(TAG, "Config written successfully");
    return ESP_OK;
}
```

---

## Performance Considerations

### 1. Memory Management

```c
// Good: Proper memory management
esp_err_t memory_efficient_function(cJSON *params, cJSON **result, void *user_data) {
    // Allocate result early
    *result = cJSON_CreateObject();
    if (!*result) {
        return ESP_ERR_NO_MEM;
    }
    
    // Use stack for temporary data
    char temp_buffer[256];
    
    // Process data without unnecessary allocations
    cJSON *data_array = cJSON_GetObjectItem(params, "data");
    if (cJSON_IsArray(data_array)) {
        int count = cJSON_GetArraySize(data_array);
        
        // Add count without creating intermediate objects
        cJSON_AddNumberToObject(*result, "count", count);
        
        // Process each item efficiently
        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_GetArrayItem(data_array, i);
            if (cJSON_IsString(item)) {
                // Process string item
                snprintf(temp_buffer, sizeof(temp_buffer), "processed_%s", item->valuestring);
                
                // Only allocate if we need to store the result
                cJSON *processed_item = cJSON_CreateString(temp_buffer);
                if (processed_item) {
                    // Add to result array (created once)
                    cJSON *result_array = cJSON_GetObjectItem(*result, "processed");
                    if (!result_array) {
                        result_array = cJSON_CreateArray();
                        cJSON_AddItemToObject(*result, "processed", result_array);
                    }
                    cJSON_AddItemToArray(result_array, processed_item);
                }
            }
        }
    }
    
    return ESP_OK;
}

// Bad: Memory inefficient
esp_err_t memory_wasteful_function(cJSON *params, cJSON **result, void *user_data) {
    // Creating many temporary JSON objects
    cJSON *temp1 = cJSON_CreateObject();
    cJSON *temp2 = cJSON_CreateObject();
    cJSON *temp3 = cJSON_CreateObject();
    
    // Processing...
    
    // Forgetting to clean up
    // Memory leak!
    
    *result = cJSON_CreateObject();
    return ESP_OK;
}
```

### 2. Fast Parameter Extraction

```c
typedef struct {
    const char *string_val;
    int int_val;
    double double_val;
    bool bool_val;
    bool has_string;
    bool has_int;
    bool has_double;
    bool has_bool;
} fast_params_t;

void extract_params_fast(cJSON *params, fast_params_t *extracted) {
    memset(extracted, 0, sizeof(fast_params_t));
    
    cJSON *item;
    cJSON_ArrayForEach(item, params) {
        const char *name = item->string;
        
        if (strcmp(name, "text") == 0 && cJSON_IsString(item)) {
            extracted->string_val = item->valuestring;
            extracted->has_string = true;
        } else if (strcmp(name, "count") == 0 && cJSON_IsNumber(item)) {
            extracted->int_val = item->valueint;
            extracted->has_int = true;
        } else if (strcmp(name, "value") == 0 && cJSON_IsNumber(item)) {
            extracted->double_val = item->valuedouble;
            extracted->has_double = true;
        } else if (strcmp(name, "enabled") == 0 && cJSON_IsBool(item)) {
            extracted->bool_val = cJSON_IsTrue(item);
            extracted->has_bool = true;
        }
    }
}

esp_err_t fast_function(cJSON *params, cJSON **result, void *user_data) {
    fast_params_t p;
    extract_params_fast(params, &p);
    
    // Quick parameter validation
    if (!p.has_string || !p.has_int) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "Missing required parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Fast processing with extracted values
    *result = cJSON_CreateObject();
    cJSON_AddStringToObject(*result, "processed_text", p.string_val);
    cJSON_AddNumberToObject(*result, "multiplied_count", p.int_val * 2);
    
    if (p.has_double) {
        cJSON_AddNumberToObject(*result, "scaled_value", p.double_val * 1.5);
    }
    
    return ESP_OK;
}
```

---

## Best Practices

### 1. Clear Function Descriptions

```c
// Good: Clear, specific description
cJSON *create_gpio_function_desc(void) {
    cJSON *func_desc = cJSON_CreateObject();
    cJSON_AddStringToObject(func_desc, "name", "control_gpio");
    cJSON_AddStringToObject(func_desc, "description", 
        "Control ESP32 GPIO pins. Can set pin mode (input/output), "
        "set output level (high/low), or read input level. "
        "Pin numbers 0-39 are supported. Returns current pin state.");
    
    // ... parameter schema
    return func_desc;
}

// Bad: Vague description
cJSON *create_bad_function_desc(void) {
    cJSON *func_desc = cJSON_CreateObject();
    cJSON_AddStringToObject(func_desc, "name", "do_stuff");
    cJSON_AddStringToObject(func_desc, "description", "Does something with pins");
    
    return func_desc;
}
```

### 2. Input Validation

```c
esp_err_t validate_gpio_params(cJSON *params, int *pin, const char **action, int *value) {
    // Validate pin number
    cJSON *pin_json = cJSON_GetObjectItem(params, "pin");
    if (!cJSON_IsNumber(pin_json)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *pin = pin_json->valueint;
    if (*pin < 0 || *pin > 39) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate action
    cJSON *action_json = cJSON_GetObjectItem(params, "action");
    if (!cJSON_IsString(action_json)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *action = action_json->valuestring;
    if (strcmp(*action, "set_output") != 0 && 
        strcmp(*action, "set_input") != 0 && 
        strcmp(*action, "read") != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate value (if needed)
    cJSON *value_json = cJSON_GetObjectItem(params, "value");
    if (strcmp(*action, "set_output") == 0) {
        if (!cJSON_IsNumber(value_json)) {
            return ESP_ERR_INVALID_ARG;
        }
        *value = value_json->valueint;
        if (*value != 0 && *value != 1) {
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        *value = -1;  // Not needed
    }
    
    return ESP_OK;
}
```

### 3. Informative Results

```c
esp_err_t informative_function(cJSON *params, cJSON **result, void *user_data) {
    *result = cJSON_CreateObject();
    
    // Add execution metadata
    cJSON_AddStringToObject(*result, "function", "informative_function");
    cJSON_AddNumberToObject(*result, "timestamp", xTaskGetTickCount() * portTICK_PERIOD_MS);
    cJSON_AddStringToObject(*result, "version", "1.0.0");
    
    // Add actual result data
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "status", "success");
    cJSON_AddNumberToObject(data, "value", 42);
    cJSON_AddItemToObject(*result, "data", data);
    
    // Add system information
    cJSON *system_info = cJSON_CreateObject();
    cJSON_AddNumberToObject(system_info, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(system_info, "uptime_ms", xTaskGetTickCount() * portTICK_PERIOD_MS);
    cJSON_AddItemToObject(*result, "system", system_info);
    
    return ESP_OK;
}
```

---

## Real-World Examples

### 1. Home Automation Controller

```c
typedef struct {
    bool lights[8];
    int brightness[8];
    float temperature;
    bool hvac_on;
} home_state_t;

esp_err_t control_lights(cJSON *params, cJSON **result, void *user_data) {
    home_state_t *home = (home_state_t*)user_data;
    
    cJSON *light_id_json = cJSON_GetObjectItem(params, "light_id");
    cJSON *action_json = cJSON_GetObjectItem(params, "action");
    cJSON *brightness_json = cJSON_GetObjectItem(params, "brightness");
    
    if (!cJSON_IsNumber(light_id_json) || !cJSON_IsString(action_json)) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    int light_id = light_id_json->valueint;
    const char *action = action_json->valuestring;
    
    if (light_id < 0 || light_id >= 8) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "Invalid light ID (0-7)");
        return ESP_ERR_INVALID_ARG;
    }
    
    *result = cJSON_CreateObject();
    
    if (strcmp(action, "turn_on") == 0) {
        home->lights[light_id] = true;
        
        if (cJSON_IsNumber(brightness_json)) {
            int brightness = brightness_json->valueint;
            if (brightness >= 0 && brightness <= 100) {
                home->brightness[light_id] = brightness;
            }
        }
        
        cJSON_AddStringToObject(*result, "action", "turned_on");
        cJSON_AddNumberToObject(*result, "light_id", light_id);
        cJSON_AddNumberToObject(*result, "brightness", home->brightness[light_id]);
        
    } else if (strcmp(action, "turn_off") == 0) {
        home->lights[light_id] = false;
        
        cJSON_AddStringToObject(*result, "action", "turned_off");
        cJSON_AddNumberToObject(*result, "light_id", light_id);
        
    } else if (strcmp(action, "get_status") == 0) {
        cJSON_AddStringToObject(*result, "action", "status");
        cJSON_AddNumberToObject(*result, "light_id", light_id);
        cJSON_AddBoolToObject(*result, "is_on", home->lights[light_id]);
        cJSON_AddNumberToObject(*result, "brightness", home->brightness[light_id]);
    }
    
    return ESP_OK;
}

esp_err_t control_hvac(cJSON *params, cJSON **result, void *user_data) {
    home_state_t *home = (home_state_t*)user_data;
    
    cJSON *action_json = cJSON_GetObjectItem(params, "action");
    cJSON *target_temp_json = cJSON_GetObjectItem(params, "target_temperature");
    
    if (!cJSON_IsString(action_json)) {
        *result = cJSON_CreateObject();
        cJSON_AddStringToObject(*result, "error", "Action required");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *action = action_json->valuestring;
    *result = cJSON_CreateObject();
    
    if (strcmp(action, "turn_on") == 0) {
        home->hvac_on = true;
        cJSON_AddStringToObject(*result, "hvac_status", "on");
        
    } else if (strcmp(action, "turn_off") == 0) {
        home->hvac_on = false;
        cJSON_AddStringToObject(*result, "hvac_status", "off");
        
    } else if (strcmp(action, "get_temperature") == 0) {
        // Simulate reading temperature sensor
        home->temperature = 22.5f + (esp_random() % 100) / 100.0f;
        cJSON_AddNumberToObject(*result, "current_temperature", home->temperature);
    }
    
    cJSON_AddBoolToObject(*result, "hvac_on", home->hvac_on);
    return ESP_OK;
}

// Register home automation functions
esp_err_t setup_home_automation(openrouter_handle_t handle) {
    static home_state_t home_state = {0};
    
    // Initialize home state
    for (int i = 0; i < 8; i++) {
        home_state.lights[i] = false;
        home_state.brightness[i] = 100;
    }
    home_state.temperature = 22.0f;
    home_state.hvac_on = false;
    
    // Register light control function
    // ... (function description creation omitted for brevity)
    
    // Register HVAC control function
    // ... (function description creation omitted for brevity)
    
    return ESP_OK;
}
```

### 2. Sensor Data Logger

```c
typedef struct {
    float sensor_data[100];
    int data_count;
    uint32_t last_reading_time;
} sensor_logger_t;

esp_err_t log_sensor_data(cJSON *params, cJSON **result, void *user_data) {
    sensor_logger_t *logger = (sensor_logger_t*)user_data;
    
    // Read current sensor value
    float current_value = read_temperature_sensor();
    
    // Add to log
    if (logger->data_count < 100) {
        logger->sensor_data[logger->data_count++] = current_value;
    } else {
        // Shift data and add new value
        memmove(logger->sensor_data, logger->sensor_data + 1, sizeof(float) * 99);
        logger->sensor_data[99] = current_value;
    }
    
    logger->last_reading_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Calculate statistics
    float sum = 0, min_val = logger->sensor_data[0], max_val = logger->sensor_data[0];
    for (int i = 0; i < logger->data_count; i++) {
        sum += logger->sensor_data[i];
        if (logger->sensor_data[i] < min_val) min_val = logger->sensor_data[i];
        if (logger->sensor_data[i] > max_val) max_val = logger->sensor_data[i];
    }
    float average = sum / logger->data_count;
    
    // Create result
    *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(*result, "current_value", current_value);
    cJSON_AddNumberToObject(*result, "data_points", logger->data_count);
    cJSON_AddNumberToObject(*result, "average", average);
    cJSON_AddNumberToObject(*result, "minimum", min_val);
    cJSON_AddNumberToObject(*result, "maximum", max_val);
    cJSON_AddNumberToObject(*result, "last_reading_time", logger->last_reading_time);
    
    return ESP_OK;
}

esp_err_t get_sensor_history(cJSON *params, cJSON **result, void *user_data) {
    sensor_logger_t *logger = (sensor_logger_t*)user_data;
    
    cJSON *count_json = cJSON_GetObjectItem(params, "count");
    int requested_count = cJSON_IsNumber(count_json) ? count_json->valueint : logger->data_count;
    
    if (requested_count > logger->data_count) {
        requested_count = logger->data_count;
    }
    
    *result = cJSON_CreateObject();
    cJSON *data_array = cJSON_CreateArray();
    
    // Return most recent data
    int start_index = logger->data_count - requested_count;
    for (int i = start_index; i < logger->data_count; i++) {
        cJSON_AddItemToArray(data_array, cJSON_CreateNumber(logger->sensor_data[i]));
    }
    
    cJSON_AddItemToObject(*result, "sensor_data", data_array);
    cJSON_AddNumberToObject(*result, "count", requested_count);
    cJSON_AddNumberToObject(*result, "total_readings", logger->data_count);
    
    return ESP_OK;
}
```

---

*For complete working examples of function calling implementations, see the [function calling example](../examples/function_calling_example/) in the examples directory.*
