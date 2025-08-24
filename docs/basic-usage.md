# Basic Usage Guide

This guide covers the fundamental concepts and basic usage patterns for the OpenRouter ESP-IDF library. Start here if you're new to the library.

## Table of Contents

- [Quick Start](#quick-start)
- [Basic Configuration](#basic-configuration)
- [Simple API Call](#simple-api-call)
- [Working with System Roles](#working-with-system-roles)
- [Parameter Tuning](#parameter-tuning)
- [Error Handling](#error-handling)
- [Best Practices](#best-practices)
- [Common Patterns](#common-patterns)

---

## Quick Start

### 1. Include the Header

```c
#include "openrouter.h"
#include "esp_log.h"

static const char *TAG = "MY_APP";
```

### 2. Basic Setup

```c
void app_main(void) {
    // Initialize Wi-Fi first (code not shown)
    wifi_init_sta();
    
    // Configure OpenRouter client
    openrouter_config_t config = {
        .api_key = "your_openrouter_api_key",
        .default_model = "openai/gpt-3.5-turbo",
        .temperature = 0.7f,
        .max_tokens = 1024,
        .response_buffer_size = 4096
    };
    
    // Create handle
    openrouter_handle_t handle = openrouter_create(&config);
    if (!handle) {
        ESP_LOGE(TAG, "Failed to create OpenRouter handle");
        return;
    }
    
    // Make API call
    char response[2048];
    esp_err_t err = openrouter_call(handle, 
        "Explain how IoT devices work in simple terms", 
        response, sizeof(response));
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "AI Response: %s", response);
    } else {
        ESP_LOGE(TAG, "API call failed: %s", esp_err_to_name(err));
    }
    
    // Cleanup
    openrouter_destroy(handle);
}
```

---

## Basic Configuration

### Minimal Configuration

The most basic configuration requires only an API key:

```c
openrouter_config_t config = {
    .api_key = "sk-or-v1-your-api-key-here"
    // All other fields will use defaults
};
```

### Recommended Configuration

For most applications, this configuration provides a good starting point:

```c
openrouter_config_t config = {
    .api_key = "sk-or-v1-your-api-key-here",
    .default_model = "openai/gpt-3.5-turbo",
    .default_system_role = "You are a helpful assistant.",
    .temperature = 0.7f,
    .max_tokens = 1024,
    .response_buffer_size = 4096,
    .enable_streaming = false,
    .http_timeout_ms = 30000
};
```

### Configuration Field Guide

| Field | Purpose | Typical Values |
|-------|---------|----------------|
| `api_key` | Authentication with OpenRouter | Get from [openrouter.ai](https://openrouter.ai) |
| `default_model` | AI model to use | `"openai/gpt-3.5-turbo"`, `"openai/gpt-4"` |
| `default_system_role` | Define AI behavior | `"You are a helpful assistant."` |
| `temperature` | Response creativity | `0.1` (focused) to `1.5` (creative) |
| `max_tokens` | Response length limit | `256` (short) to `4096` (long) |
| `response_buffer_size` | Internal buffer size | `2048` to `8192` bytes |

---

## Simple API Call

### Basic Pattern

```c
esp_err_t make_api_call(openrouter_handle_t handle) {
    char response[2048];
    
    esp_err_t err = openrouter_call(handle,
        "What are the key features of ESP32?",
        response, sizeof(response));
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Response: %s", response);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "API call failed: %s", esp_err_to_name(err));
        return err;
    }
}
```

### With Error Handling

```c
esp_err_t robust_api_call(openrouter_handle_t handle, const char *prompt) {
    char *response = NULL;
    esp_err_t err = ESP_FAIL;
    
    // Allocate response buffer
    response = malloc(4096);
    if (!response) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        return ESP_ERR_NO_MEM;
    }
    
    // Make API call
    err = openrouter_call(handle, prompt, response, 4096);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Success: %s", response);
    } else {
        ESP_LOGE(TAG, "API call failed: %s", esp_err_to_name(err));
        
        // Handle specific errors
        switch (err) {
            case ESP_ERR_NO_MEM:
                ESP_LOGE(TAG, "Out of memory - try smaller response buffer");
                break;
            case ESP_ERR_TIMEOUT:
                ESP_LOGE(TAG, "Request timed out - check network connection");
                break;
            default:
                ESP_LOGE(TAG, "Unknown error occurred");
                break;
        }
    }
    
    free(response);
    return err;
}
```

### Dynamic Buffer Sizing

```c
esp_err_t adaptive_api_call(openrouter_handle_t handle, const char *prompt) {
    size_t buffer_size = 1024;
    char *response = NULL;
    esp_err_t err;
    
    // Try progressively larger buffers
    for (int attempt = 0; attempt < 3; attempt++) {
        response = malloc(buffer_size);
        if (!response) {
            ESP_LOGE(TAG, "Failed to allocate %zu bytes", buffer_size);
            return ESP_ERR_NO_MEM;
        }
        
        err = openrouter_call(handle, prompt, response, buffer_size);
        
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Success with %zu byte buffer: %s", buffer_size, response);
            free(response);
            return ESP_OK;
        } else if (err == ESP_ERR_NO_MEM || strstr(esp_err_to_name(err), "buffer")) {
            // Response too large, try bigger buffer
            ESP_LOGW(TAG, "Buffer too small (%zu bytes), trying %zu", 
                     buffer_size, buffer_size * 2);
            free(response);
            buffer_size *= 2;
            continue;
        } else {
            // Different error, don't retry
            ESP_LOGE(TAG, "API call failed: %s", esp_err_to_name(err));
            free(response);
            return err;
        }
    }
    
    ESP_LOGE(TAG, "Failed after trying buffers up to %zu bytes", buffer_size);
    return ESP_ERR_NO_MEM;
}
```

---

## Working with System Roles

System roles define the AI's behavior and personality. They're like instructions that persist across the conversation.

### Setting System Role in Configuration

```c
openrouter_config_t config = {
    .api_key = "your_api_key",
    .default_model = "openai/gpt-3.5-turbo",
    .default_system_role = "You are an expert ESP32 developer who explains concepts clearly and provides practical code examples.",
    // ... other config
};
```

### Changing System Role at Runtime

```c
esp_err_t set_specialist_mode(openrouter_handle_t handle) {
    esp_err_t err = openrouter_set_system_role(handle,
        "You are a hardware troubleshooting specialist. "
        "Provide step-by-step diagnostic procedures for ESP32 issues.");
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Switched to hardware specialist mode");
    }
    return err;
}
```

### One-time System Role Override

```c
esp_err_t ask_coding_question(openrouter_handle_t handle) {
    char response[4096];
    
    // Use specific system role for this call only
    esp_err_t err = openrouter_call_with_system(handle,
        "You are a senior C programmer specializing in embedded systems. "
        "Provide concise, production-ready code examples.",
        "How do I implement a non-blocking UART reader in ESP-IDF?",
        response, sizeof(response));
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Coding advice: %s", response);
    }
    return err;
}
```

### Effective System Role Examples

```c
// For IoT applications
const char *iot_assistant = 
    "You are an IoT development assistant. Provide practical advice for "
    "ESP32 projects including hardware connections, code examples, and "
    "troubleshooting tips. Keep responses concise and actionable.";

// For educational content
const char *teacher_role = 
    "You are a patient teacher explaining embedded systems concepts. "
    "Use simple language, provide analogies, and break complex topics "
    "into digestible steps.";

// For debugging help
const char *debugger_role = 
    "You are a debugging expert. When presented with code or error messages, "
    "provide systematic troubleshooting steps and explain the likely causes.";

// For code review
const char *reviewer_role = 
    "You are a code reviewer focused on embedded C best practices. "
    "Analyze code for potential issues, suggest improvements, and "
    "explain the reasoning behind your recommendations.";
```

---

## Parameter Tuning

### Temperature Control

Temperature controls how creative vs. focused the responses are:

```c
// Very focused, deterministic responses
openrouter_set_temperature(handle, 0.1f);
esp_err_t err = openrouter_call(handle, 
    "What is the exact voltage range for ESP32 GPIO pins?", 
    response, sizeof(response));

// Balanced responses (default)
openrouter_set_temperature(handle, 0.7f);
err = openrouter_call(handle, 
    "Suggest creative IoT project ideas using ESP32", 
    response, sizeof(response));

// Very creative responses
openrouter_set_temperature(handle, 1.5f);
err = openrouter_call(handle, 
    "Write a creative story about an ESP32 that becomes self-aware", 
    response, sizeof(response));
```

### Token Length Control

```c
// Short, concise responses
openrouter_set_max_tokens(handle, 100);
esp_err_t err = openrouter_call(handle, 
    "Briefly explain what PWM is", 
    response, sizeof(response));

// Detailed explanations
openrouter_set_max_tokens(handle, 1500);
err = openrouter_call(handle, 
    "Explain PWM in detail with ESP32 examples", 
    response, sizeof(response));
```

### Top-p Sampling

```c
// More focused sampling (recommended for factual questions)
openrouter_set_top_p(handle, 0.1f);
esp_err_t err = openrouter_call(handle, 
    "What is the flash memory size of ESP32-S3?", 
    response, sizeof(response));

// Broader sampling (good for creative tasks)
openrouter_set_top_p(handle, 0.9f);
err = openrouter_call(handle, 
    "Brainstorm unique sensor combinations for an environmental monitor", 
    response, sizeof(response));
```

### Deterministic Responses

```c
// Set seed for reproducible outputs
openrouter_set_seed(handle, 12345);

// This will always produce the same response given the same input
esp_err_t err = openrouter_call(handle, 
    "Generate a random 6-digit device ID", 
    response, sizeof(response));
```

---

## Error Handling

### Basic Error Checking

```c
esp_err_t err = openrouter_call(handle, prompt, response, sizeof(response));

if (err != ESP_OK) {
    ESP_LOGE(TAG, "API call failed: %s", esp_err_to_name(err));
    return err;
}
```

### Comprehensive Error Handling

```c
esp_err_t handle_api_errors(esp_err_t err) {
    switch (err) {
        case ESP_OK:
            return ESP_OK;
            
        case ESP_ERR_INVALID_ARG:
            ESP_LOGE(TAG, "Invalid arguments provided to API call");
            break;
            
        case ESP_ERR_NO_MEM:
            ESP_LOGE(TAG, "Out of memory - response may be too large");
            break;
            
        case ESP_ERR_TIMEOUT:
            ESP_LOGE(TAG, "Request timed out - check network connection");
            break;
            
        case ESP_ERR_HTTP_CONNECT:
            ESP_LOGE(TAG, "HTTP connection failed - check internet connectivity");
            break;
            
        case ESP_FAIL:
            ESP_LOGE(TAG, "General failure - check API key and request format");
            break;
            
        default:
            ESP_LOGE(TAG, "Unknown error: %s", esp_err_to_name(err));
            break;
    }
    return err;
}

// Usage
esp_err_t err = openrouter_call(handle, prompt, response, sizeof(response));
handle_api_errors(err);
```

### Retry Logic

```c
esp_err_t api_call_with_retry(openrouter_handle_t handle, const char *prompt, 
                             char *response, size_t response_size) {
    const int max_retries = 3;
    const int retry_delay_ms = 1000;
    
    for (int attempt = 0; attempt < max_retries; attempt++) {
        esp_err_t err = openrouter_call(handle, prompt, response, response_size);
        
        if (err == ESP_OK) {
            if (attempt > 0) {
                ESP_LOGI(TAG, "API call succeeded on attempt %d", attempt + 1);
            }
            return ESP_OK;
        }
        
        // Don't retry on certain errors
        if (err == ESP_ERR_INVALID_ARG || err == ESP_ERR_NO_MEM) {
            ESP_LOGE(TAG, "Non-recoverable error: %s", esp_err_to_name(err));
            return err;
        }
        
        if (attempt < max_retries - 1) {
            ESP_LOGW(TAG, "API call failed (attempt %d), retrying in %d ms: %s", 
                     attempt + 1, retry_delay_ms, esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
        }
    }
    
    ESP_LOGE(TAG, "API call failed after %d attempts", max_retries);
    return ESP_FAIL;
}
```

---

## Best Practices

### 1. Resource Management

```c
void good_resource_management() {
    openrouter_handle_t handle = NULL;
    char *response = NULL;
    
    // Create handle
    openrouter_config_t config = { /* ... */ };
    handle = openrouter_create(&config);
    if (!handle) {
        goto cleanup;
    }
    
    // Allocate response buffer
    response = malloc(4096);
    if (!response) {
        goto cleanup;
    }
    
    // Use the API
    esp_err_t err = openrouter_call(handle, "Hello", response, 4096);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Response: %s", response);
    }
    
cleanup:
    // Always cleanup resources
    if (response) free(response);
    if (handle) openrouter_destroy(handle);
}
```

### 2. Configuration Management

```c
// Store configuration in a reusable structure
typedef struct {
    openrouter_handle_t handle;
    char response_buffer[4096];
    bool initialized;
} app_ai_context_t;

esp_err_t init_ai_context(app_ai_context_t *ctx, const char *api_key) {
    if (!ctx || !api_key) return ESP_ERR_INVALID_ARG;
    
    openrouter_config_t config = {
        .api_key = api_key,
        .default_model = "openai/gpt-3.5-turbo",
        .temperature = 0.7f,
        .max_tokens = 1024,
        .response_buffer_size = sizeof(ctx->response_buffer)
    };
    
    ctx->handle = openrouter_create(&config);
    if (!ctx->handle) {
        return ESP_FAIL;
    }
    
    ctx->initialized = true;
    return ESP_OK;
}

void cleanup_ai_context(app_ai_context_t *ctx) {
    if (ctx && ctx->initialized) {
        openrouter_destroy(ctx->handle);
        ctx->initialized = false;
    }
}
```

### 3. Prompt Engineering

```c
// Good: Specific, clear prompts
const char *good_prompt = 
    "Explain how to configure ESP32 GPIO pin 2 as an output pin and "
    "turn on an LED connected to it. Provide the exact C code.";

// Bad: Vague prompts
const char *bad_prompt = "How to use GPIO?";

// Good: Structured prompts for complex tasks
const char *structured_prompt = 
    "Task: Design a temperature monitoring system\n"
    "Requirements:\n"
    "- Use DS18B20 temperature sensor\n" 
    "- Send data via WiFi every 5 minutes\n"
    "- Include error handling\n"
    "Please provide the main application structure in C.";
```

### 4. Buffer Sizing

```c
// Size buffers appropriately for your use case
#define SHORT_RESPONSE_SIZE 512      // For simple questions
#define MEDIUM_RESPONSE_SIZE 2048    // For explanations
#define LONG_RESPONSE_SIZE 4096      // For code examples
#define MAX_RESPONSE_SIZE 8192       // For detailed tutorials

esp_err_t ask_simple_question(openrouter_handle_t handle) {
    char response[SHORT_RESPONSE_SIZE];
    return openrouter_call(handle, "What is I2C?", response, sizeof(response));
}

esp_err_t get_code_example(openrouter_handle_t handle) {
    char response[LONG_RESPONSE_SIZE];
    return openrouter_call(handle, 
        "Provide a complete C example of ESP32 WiFi station setup",
        response, sizeof(response));
}
```

---

## Common Patterns

### 1. Query-Response Pattern

```c
typedef struct {
    const char *question;
    char response[2048];
    bool answered;
} qa_pair_t;

esp_err_t process_questions(openrouter_handle_t handle, qa_pair_t *questions, int count) {
    for (int i = 0; i < count; i++) {
        esp_err_t err = openrouter_call(handle, 
            questions[i].question, 
            questions[i].response, 
            sizeof(questions[i].response));
        
        questions[i].answered = (err == ESP_OK);
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to answer question %d: %s", i, esp_err_to_name(err));
            return err;
        }
        
        // Small delay between requests to be respectful
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return ESP_OK;
}
```

### 2. Configuration-Based Response Pattern

```c
typedef enum {
    AI_MODE_BEGINNER,
    AI_MODE_INTERMEDIATE, 
    AI_MODE_EXPERT
} ai_difficulty_mode_t;

esp_err_t ask_with_difficulty(openrouter_handle_t handle, const char *question, 
                             ai_difficulty_mode_t mode, char *response, size_t response_size) {
    const char *system_roles[] = {
        [AI_MODE_BEGINNER] = "Explain concepts simply with basic examples",
        [AI_MODE_INTERMEDIATE] = "Provide detailed explanations with practical examples",
        [AI_MODE_EXPERT] = "Give comprehensive technical details and advanced examples"
    };
    
    return openrouter_call_with_system(handle, 
        system_roles[mode], question, response, response_size);
}
```

### 3. Context-Aware Pattern

```c
typedef struct {
    char context[1024];
    openrouter_handle_t handle;
} contextual_ai_t;

esp_err_t add_context(contextual_ai_t *ai, const char *new_info) {
    size_t current_len = strlen(ai->context);
    size_t new_len = strlen(new_info);
    
    if (current_len + new_len + 2 < sizeof(ai->context)) {
        strcat(ai->context, " ");
        strcat(ai->context, new_info);
        return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t ask_with_context(contextual_ai_t *ai, const char *question, 
                          char *response, size_t response_size) {
    char full_prompt[2048];
    snprintf(full_prompt, sizeof(full_prompt), 
        "Context: %s\n\nQuestion: %s", ai->context, question);
    
    return openrouter_call(ai->handle, full_prompt, response, response_size);
}
```

---

## Next Steps

Once you're comfortable with basic usage, explore these advanced features:

1. **[Streaming Responses](streaming.md)** - Real-time token delivery
2. **[Function Calling](function-calling.md)** - Let AI call your functions
3. **[Multimodal Support](multimodal.md)** - Process images and audio
4. **[Configuration Guide](configuration.md)** - Advanced configuration options

For practical examples, check out the [examples directory](../examples/) which demonstrates these concepts in working applications.
