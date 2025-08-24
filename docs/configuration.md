# Configuration Guide

This comprehensive guide covers all configuration options, menuconfig settings, and advanced configuration patterns for the OpenRouter ESP-IDF library.

## Table of Contents

- [Menuconfig Options](#menuconfig-options)
- [Runtime Configuration](#runtime-configuration)
- [Model Selection](#model-selection)
- [Performance Tuning](#performance-tuning)
- [Security Configuration](#security-configuration)
- [Advanced Settings](#advanced-settings)
- [Configuration Patterns](#configuration-patterns)
- [Environment-Specific Configs](#environment-specific-configs)

---

## Menuconfig Options

Access configuration options through ESP-IDF's menuconfig system:

```bash
idf.py menuconfig
```

Navigate to: **Component config → OpenRouter Client**

### Core Settings

#### Response Buffer Size
```
CONFIG_OPENROUTER_DEFAULT_RESPONSE_BUFFER_SIZE
Default: 4096
Range: 512 - 65536
```

Controls the default size of internal response buffers. Larger values allow for longer responses but use more memory.

**Recommendations:**
- **Small projects**: 2048 bytes
- **General use**: 4096 bytes (default)
- **Long responses**: 8192 bytes
- **Memory constrained**: 1024 bytes

#### HTTP Timeouts
```
CONFIG_OPENROUTER_DEFAULT_HTTP_TIMEOUT
Default: 30000 (30 seconds)
Range: 5000 - 300000
```

Timeout for regular HTTP requests in milliseconds.

```
CONFIG_OPENROUTER_DEFAULT_HTTP_TIMEOUT_STREAMING
Default: 60000 (60 seconds)
Range: 10000 - 600000
```

Timeout for streaming requests (typically longer due to real-time nature).

### Model Parameters

#### Temperature
```
CONFIG_OPENROUTER_DEFAULT_TEMPERATURE
Default: "0.7"
Range: "0.0" - "2.0"
```

Default creativity level for AI responses:
- `"0.0"` - Most focused and deterministic
- `"0.7"` - Balanced (recommended)
- `"1.0"` - More creative
- `"2.0"` - Most creative and random

#### Maximum Tokens
```
CONFIG_OPENROUTER_DEFAULT_MAX_TOKENS
Default: 1024
Range: 1 - 8192
```

Default maximum number of tokens in AI responses.

#### Top-P Sampling
```
CONFIG_OPENROUTER_DEFAULT_TOP_P
Default: "1.0"
Range: "0.0" - "1.0"
```

Nucleus sampling parameter:
- `"0.1"` - Only top 10% probability tokens
- `"1.0"` - All tokens considered (default)

### Connection Settings

#### Connection Reuse
```
CONFIG_OPENROUTER_DEFAULT_CONNECTION_REUSE
Default: disabled
```

Enable HTTP connection pooling for better performance with multiple requests.

#### Connection Timeout
```
CONFIG_OPENROUTER_DEFAULT_CONNECTION_TIMEOUT
Default: 30000 (30 seconds)
Range: 5000 - 300000
```

How long to keep connections alive for reuse.

### Debugging Options

#### Verbose Logging
```
CONFIG_OPENROUTER_VERBOSE_LOGGING
Default: disabled
```

Enable detailed debug logging including:
- HTTP request/response details
- JSON parsing information
- Memory allocation tracking
- Streaming token details

**Warning**: Enabling this will significantly increase log output and may expose sensitive data.

---

## Runtime Configuration

### Configuration Structure

```c
typedef struct {
    const char *api_key;                    // Required: OpenRouter API key
    const char *default_model;              // AI model identifier
    const char *default_system_role;        // System role/instructions
    float temperature;                      // Response randomness (0.0-2.0)
    int max_tokens;                        // Maximum response tokens
    float top_p;                           // Top-p sampling (0.0-1.0)
    int seed;                              // Random seed (-1 for random)
    size_t response_buffer_size;           // Internal buffer size
    bool enable_streaming;                 // Enable streaming mode
    bool enable_tools;                     // Enable function calling
    uint32_t http_timeout_ms;              // HTTP timeout
    bool enable_connection_reuse;          // Enable connection pooling
    uint32_t connection_timeout_ms;        // Connection keep-alive timeout
    const char *http_referer;              // HTTP-Referer header
    const char *x_title;                   // X-Title header for tracking
    openrouter_tool_callback_t tool_callback;  // Tool callback function
    void *tool_user_data;                  // User data for tools
} openrouter_config_t;
```

### Basic Configuration Examples

#### Minimal Configuration
```c
openrouter_config_t config = {
    .api_key = "sk-or-v1-your-api-key-here"
    // All other fields will use defaults
};
```

#### Balanced Configuration
```c
openrouter_config_t config = {
    .api_key = "sk-or-v1-your-api-key-here",
    .default_model = "openai/gpt-3.5-turbo",
    .default_system_role = "You are a helpful IoT assistant.",
    .temperature = 0.7f,
    .max_tokens = 1024,
    .top_p = 1.0f,
    .seed = -1,
    .response_buffer_size = 4096,
    .enable_streaming = false,
    .http_timeout_ms = 30000
};
```

#### High-Performance Configuration
```c
openrouter_config_t config = {
    .api_key = "sk-or-v1-your-api-key-here",
    .default_model = "openai/gpt-3.5-turbo",
    .temperature = 0.7f,
    .max_tokens = 1024,
    .response_buffer_size = 8192,
    .enable_streaming = true,
    .enable_connection_reuse = true,
    .connection_timeout_ms = 60000,
    .http_timeout_ms = 45000,
    .http_referer = "https://myiot.device.com"
};
```

### Dynamic Configuration Updates

```c
esp_err_t update_configuration_for_task(openrouter_handle_t handle, const char *task_type) {
    if (strcmp(task_type, "creative") == 0) {
        openrouter_set_temperature(handle, 1.2f);
        openrouter_set_max_tokens(handle, 2048);
        openrouter_set_model(handle, "anthropic/claude-3-sonnet");
        
    } else if (strcmp(task_type, "factual") == 0) {
        openrouter_set_temperature(handle, 0.1f);
        openrouter_set_max_tokens(handle, 512);
        openrouter_set_model(handle, "openai/gpt-4");
        
    } else if (strcmp(task_type, "code") == 0) {
        openrouter_set_temperature(handle, 0.2f);
        openrouter_set_max_tokens(handle, 4096);
        openrouter_set_model(handle, "openai/gpt-4");
        openrouter_set_system_role(handle, 
            "You are an expert C programmer for embedded systems.");
    }
    
    return ESP_OK;
}
```

---

## Model Selection

### Available Model Categories

#### Fast & Cost-Effective Models
```c
// Good for simple tasks, high throughput
const char *fast_models[] = {
    "openai/gpt-3.5-turbo",
    "anthropic/claude-3-haiku",
    "google/gemini-flash",
    "deepseek/deepseek-chat-v3-0324:free"
};
```

#### Balanced Performance Models
```c
// Good balance of quality and speed
const char *balanced_models[] = {
    "openai/gpt-4o-mini",
    "anthropic/claude-3-sonnet",
    "google/gemini-pro",
    "meta-llama/llama-3-8b-instruct"
};
```

#### High-Quality Models
```c
// Best quality, higher cost
const char *premium_models[] = {
    "openai/gpt-4",
    "openai/gpt-4-turbo",
    "anthropic/claude-3.5-sonnet",
    "google/gemini-pro-1.5"
};
```

#### Specialized Models
```c
// For specific use cases
const char *code_models[] = {
    "openai/gpt-4",
    "anthropic/claude-3-sonnet",
    "deepseek/deepseek-coder"
};

const char *multimodal_models[] = {
    "openai/gpt-4-vision-preview",
    "openai/gpt-4o",
    "google/gemini-pro-vision",
    "anthropic/claude-3-sonnet"
};
```

### Model Selection Strategy

```c
typedef enum {
    TASK_TYPE_SIMPLE_QA,
    TASK_TYPE_CREATIVE,
    TASK_TYPE_TECHNICAL,
    TASK_TYPE_CODE_GENERATION,
    TASK_TYPE_MULTIMODAL
} task_type_t;

const char* select_optimal_model(task_type_t task, bool cost_sensitive) {
    switch (task) {
        case TASK_TYPE_SIMPLE_QA:
            return cost_sensitive ? "openai/gpt-3.5-turbo" : "openai/gpt-4o-mini";
            
        case TASK_TYPE_CREATIVE:
            return cost_sensitive ? "anthropic/claude-3-haiku" : "anthropic/claude-3.5-sonnet";
            
        case TASK_TYPE_TECHNICAL:
            return cost_sensitive ? "openai/gpt-4o-mini" : "openai/gpt-4";
            
        case TASK_TYPE_CODE_GENERATION:
            return cost_sensitive ? "deepseek/deepseek-coder" : "openai/gpt-4";
            
        case TASK_TYPE_MULTIMODAL:
            return cost_sensitive ? "openai/gpt-4o-mini" : "openai/gpt-4o";
            
        default:
            return "openai/gpt-3.5-turbo";
    }
}

esp_err_t configure_for_task(openrouter_handle_t handle, task_type_t task, bool cost_sensitive) {
    const char *model = select_optimal_model(task, cost_sensitive);
    esp_err_t err = openrouter_set_model(handle, model);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Configured for task type %d with model: %s", task, model);
    }
    
    return err;
}
```

---

## Performance Tuning

### Memory Optimization

```c
// Memory-constrained configuration
openrouter_config_t memory_optimized_config = {
    .api_key = "your_api_key",
    .default_model = "openai/gpt-3.5-turbo",  // Efficient model
    .temperature = 0.7f,
    .max_tokens = 512,                        // Limit response length
    .response_buffer_size = 1024,             // Smaller buffer
    .enable_streaming = true,                 // Reduce memory usage
    .http_timeout_ms = 20000                  // Shorter timeout
};
```

### Speed Optimization

```c
// High-speed configuration
openrouter_config_t speed_optimized_config = {
    .api_key = "your_api_key",
    .default_model = "openai/gpt-3.5-turbo",  // Fast model
    .temperature = 0.3f,                      // Less randomness = faster
    .max_tokens = 256,                        // Shorter responses
    .enable_streaming = true,                 // Start processing immediately
    .enable_connection_reuse = true,          // Avoid connection overhead
    .connection_timeout_ms = 30000,
    .http_timeout_ms = 15000                  // Shorter timeout
};
```

### Throughput Optimization

```c
// High-throughput configuration for multiple requests
openrouter_config_t throughput_optimized_config = {
    .api_key = "your_api_key",
    .default_model = "openai/gpt-3.5-turbo",
    .temperature = 0.7f,
    .max_tokens = 1024,
    .response_buffer_size = 8192,             // Larger buffer for batch processing
    .enable_connection_reuse = true,          // Essential for throughput
    .connection_timeout_ms = 120000,          // Longer keep-alive
    .http_timeout_ms = 60000
};
```

### Buffer Size Guidelines

| Use Case | Buffer Size | Rationale |
|----------|-------------|-----------|
| Short answers | 512-1024 bytes | Simple Q&A, status messages |
| Explanations | 2048-4096 bytes | Technical explanations, tutorials |
| Code generation | 4096-8192 bytes | Complete functions, examples |
| Long content | 8192+ bytes | Documentation, detailed analysis |

---

## Security Configuration

### API Key Management

```c
// Secure API key handling
#define API_KEY_MAX_LENGTH 256

typedef struct {
    char encrypted_key[API_KEY_MAX_LENGTH];
    bool key_validated;
    uint32_t last_validation_time;
} secure_config_t;

esp_err_t set_secure_api_key(secure_config_t *config, const char *api_key) {
    if (!config || !api_key) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Basic validation
    if (strlen(api_key) < 20 || strncmp(api_key, "sk-or-v1-", 9) != 0) {
        ESP_LOGE(TAG, "API key format invalid");
        return ESP_ERR_INVALID_ARG;
    }
    
    // In production, encrypt the key here
    strncpy(config->encrypted_key, api_key, sizeof(config->encrypted_key) - 1);
    config->key_validated = true;
    config->last_validation_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    ESP_LOGI(TAG, "API key configured securely");
    return ESP_OK;
}
```

### Header Configuration for Tracking

```c
// Configure tracking headers for monitoring
openrouter_config_t create_tracked_config(const char *api_key, const char *device_id) {
    static char referer_buffer[128];
    static char title_buffer[64];
    
    snprintf(referer_buffer, sizeof(referer_buffer), 
             "https://device.%s.iot", device_id);
    snprintf(title_buffer, sizeof(title_buffer), 
             "ESP32-Device-%s", device_id);
    
    openrouter_config_t config = {
        .api_key = api_key,
        .default_model = "openai/gpt-3.5-turbo",
        .temperature = 0.7f,
        .max_tokens = 1024,
        .response_buffer_size = 4096,
        .http_referer = referer_buffer,
        .x_title = title_buffer
    };
    
    return config;
}
```

### Rate Limiting Configuration

```c
typedef struct {
    uint32_t max_requests_per_minute;
    uint32_t request_count;
    uint32_t window_start_time;
} rate_limiter_t;

esp_err_t check_rate_limit(rate_limiter_t *limiter) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Reset window if minute has passed
    if (current_time - limiter->window_start_time >= 60000) {
        limiter->request_count = 0;
        limiter->window_start_time = current_time;
    }
    
    if (limiter->request_count >= limiter->max_requests_per_minute) {
        ESP_LOGW(TAG, "Rate limit exceeded, blocking request");
        return ESP_ERR_INVALID_STATE;
    }
    
    limiter->request_count++;
    return ESP_OK;
}
```

---

## Advanced Settings

### Custom HTTP Headers

```c
// Configure custom headers for enterprise usage
typedef struct {
    char user_agent[128];
    char custom_header_1[256];
    char custom_header_2[256];
} custom_headers_t;

esp_err_t configure_enterprise_headers(openrouter_config_t *config, 
                                      const char *org_id, const char *project_id) {
    static custom_headers_t headers;
    
    snprintf(headers.user_agent, sizeof(headers.user_agent),
             "ESP32-OpenRouter/1.0 (Org:%s Project:%s)", org_id, project_id);
    
    snprintf(headers.custom_header_1, sizeof(headers.custom_header_1),
             "X-Organization-ID: %s", org_id);
             
    snprintf(headers.custom_header_2, sizeof(headers.custom_header_2),
             "X-Project-ID: %s", project_id);
    
    // These would be used in HTTP client configuration
    ESP_LOGI(TAG, "Enterprise headers configured for org: %s", org_id);
    return ESP_OK;
}
```

### Environment Detection

```c
typedef enum {
    ENV_DEVELOPMENT,
    ENV_TESTING,
    ENV_PRODUCTION
} environment_t;

openrouter_config_t create_environment_config(environment_t env, const char *api_key) {
    openrouter_config_t config = {
        .api_key = api_key,
        .temperature = 0.7f,
        .response_buffer_size = 4096
    };
    
    switch (env) {
        case ENV_DEVELOPMENT:
            config.default_model = "openai/gpt-3.5-turbo";  // Cheaper for testing
            config.max_tokens = 512;                        // Shorter responses
            config.http_timeout_ms = 15000;                 // Shorter timeout
            config.enable_connection_reuse = false;         // Simpler debugging
            break;
            
        case ENV_TESTING:
            config.default_model = "openai/gpt-3.5-turbo";
            config.max_tokens = 1024;
            config.http_timeout_ms = 30000;
            config.enable_connection_reuse = true;
            config.seed = 12345;                            // Deterministic for tests
            break;
            
        case ENV_PRODUCTION:
            config.default_model = "openai/gpt-4o-mini";    // Better quality
            config.max_tokens = 2048;                       // Longer responses
            config.http_timeout_ms = 60000;                 // More patience
            config.enable_connection_reuse = true;          // Performance
            config.connection_timeout_ms = 120000;
            break;
    }
    
    return config;
}
```

---

## Configuration Patterns

### Configuration Factory Pattern

```c
typedef struct {
    const char *name;
    openrouter_config_t (*create_config)(const char *api_key);
} config_preset_t;

openrouter_config_t create_chatbot_config(const char *api_key) {
    return (openrouter_config_t) {
        .api_key = api_key,
        .default_model = "openai/gpt-3.5-turbo",
        .default_system_role = "You are a helpful assistant that gives concise, friendly responses.",
        .temperature = 0.8f,
        .max_tokens = 512,
        .enable_streaming = true,
        .response_buffer_size = 2048
    };
}

openrouter_config_t create_code_assistant_config(const char *api_key) {
    return (openrouter_config_t) {
        .api_key = api_key,
        .default_model = "openai/gpt-4",
        .default_system_role = "You are an expert C programmer for embedded systems. Provide clear, working code examples.",
        .temperature = 0.2f,
        .max_tokens = 4096,
        .enable_streaming = false,
        .response_buffer_size = 8192
    };
}

static const config_preset_t presets[] = {
    {"chatbot", create_chatbot_config},
    {"code_assistant", create_code_assistant_config},
    {NULL, NULL}
};

openrouter_config_t get_preset_config(const char *preset_name, const char *api_key) {
    for (int i = 0; presets[i].name != NULL; i++) {
        if (strcmp(presets[i].name, preset_name) == 0) {
            return presets[i].create_config(api_key);
        }
    }
    
    // Return default config if preset not found
    ESP_LOGW(TAG, "Preset '%s' not found, using default", preset_name);
    return (openrouter_config_t) {
        .api_key = api_key,
        .default_model = "openai/gpt-3.5-turbo"
    };
}
```

### Configuration Validation

```c
esp_err_t validate_config(const openrouter_config_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate API key
    if (!config->api_key || strlen(config->api_key) < 20) {
        ESP_LOGE(TAG, "Invalid API key");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate temperature range
    if (config->temperature < 0.0f || config->temperature > 2.0f) {
        ESP_LOGE(TAG, "Temperature out of range: %f", config->temperature);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate top_p range
    if (config->top_p < 0.0f || config->top_p > 1.0f) {
        ESP_LOGE(TAG, "Top_p out of range: %f", config->top_p);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate buffer size
    if (config->response_buffer_size < 256 || config->response_buffer_size > 65536) {
        ESP_LOGE(TAG, "Buffer size out of range: %zu", config->response_buffer_size);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate timeout values
    if (config->http_timeout_ms < 5000 || config->http_timeout_ms > 300000) {
        ESP_LOGE(TAG, "HTTP timeout out of range: %lu", config->http_timeout_ms);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Configuration validation passed");
    return ESP_OK;
}
```

---

## Environment-Specific Configs

### Development Configuration

```c
// For development and debugging
openrouter_config_t create_dev_config(const char *api_key) {
    return (openrouter_config_t) {
        .api_key = api_key,
        .default_model = "deepseek/deepseek-chat-v3-0324:free",  // Free model
        .default_system_role = "You are a development assistant. Be verbose and educational.",
        .temperature = 0.8f,                    // More creative for exploration
        .max_tokens = 2048,                     // Longer explanations
        .top_p = 1.0f,
        .seed = -1,                            // Random for variety
        .response_buffer_size = 8192,          // Large buffer for detailed responses
        .enable_streaming = true,              // Good for interactive development
        .http_timeout_ms = 60000,              // Patient timeout
        .enable_connection_reuse = false       // Simpler debugging
    };
}
```

### Production Configuration

```c
// For production deployment
openrouter_config_t create_prod_config(const char *api_key) {
    return (openrouter_config_t) {
        .api_key = api_key,
        .default_model = "openai/gpt-3.5-turbo",
        .default_system_role = "You are a helpful assistant. Be concise and accurate.",
        .temperature = 0.3f,                    // More deterministic
        .max_tokens = 1024,                     // Reasonable limit
        .top_p = 0.9f,                         // Focused responses
        .seed = 42,                            // Deterministic for consistency
        .response_buffer_size = 4096,          // Balanced buffer size
        .enable_streaming = true,              // Better user experience
        .http_timeout_ms = 30000,              // Reasonable timeout
        .enable_connection_reuse = true,       // Performance optimization
        .connection_timeout_ms = 60000
    };
}
```

### Testing Configuration

```c
// For automated testing
openrouter_config_t create_test_config(const char *api_key) {
    return (openrouter_config_t) {
        .api_key = api_key,
        .default_model = "openai/gpt-3.5-turbo",
        .default_system_role = "You are a test assistant. Give consistent, predictable responses.",
        .temperature = 0.0f,                    // Maximum determinism
        .max_tokens = 512,                      // Short, predictable responses
        .top_p = 0.1f,                         // Very focused
        .seed = 12345,                         // Fixed seed for reproducibility
        .response_buffer_size = 2048,          // Smaller buffer for tests
        .enable_streaming = false,             // Simpler for testing
        .http_timeout_ms = 15000,              // Shorter timeout for fast tests
        .enable_connection_reuse = false       // Avoid connection state issues
    };
}
```

---

*For more configuration examples, see the [examples directory](../examples/) which demonstrates various configuration patterns in working applications.*
