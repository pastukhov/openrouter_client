# Error Handling Guide

This guide covers comprehensive error handling strategies for the OpenRouter ESP-IDF library, including error codes, debugging techniques, and recovery patterns.

## Table of Contents

- [Error Code Reference](#error-code-reference)
- [Error Handling Patterns](#error-handling-patterns)
- [Debugging Techniques](#debugging-techniques)
- [Recovery Strategies](#recovery-strategies)
- [Common Issues](#common-issues)
- [Troubleshooting Guide](#troubleshooting-guide)
- [Best Practices](#best-practices)

---

## Error Code Reference

### ESP-IDF Standard Error Codes

These are standard ESP-IDF error codes that the library may return:

| Error Code | Value | Description | Common Causes |
|------------|-------|-------------|---------------|
| `ESP_OK` | 0 | Success | Operation completed successfully |
| `ESP_FAIL` | -1 | Generic failure | API errors, invalid responses |
| `ESP_ERR_INVALID_ARG` | 0x102 | Invalid argument | NULL pointers, invalid parameters |
| `ESP_ERR_NO_MEM` | 0x101 | Out of memory | Large responses, memory leaks |
| `ESP_ERR_TIMEOUT` | 0x107 | Operation timeout | Network issues, slow responses |
| `ESP_ERR_NOT_FOUND` | 0x105 | Resource not found | Invalid function names |

### OpenRouter-Specific Error Codes

The library defines custom error codes for specific conditions:

```c
#define ESP_ERR_OPENROUTER_BASE 0x9000
#define ESP_ERR_OPENROUTER_MEMORY (ESP_ERR_OPENROUTER_BASE + 1)
#define ESP_ERR_OPENROUTER_JSON_PARSE (ESP_ERR_OPENROUTER_BASE + 2)
#define ESP_ERR_OPENROUTER_API_ERROR (ESP_ERR_OPENROUTER_BASE + 3)
#define ESP_ERR_OPENROUTER_HTTP_ERROR (ESP_ERR_OPENROUTER_BASE + 4)
#define ESP_ERR_OPENROUTER_FUNCTION_EXISTS (ESP_ERR_OPENROUTER_BASE + 5)
#define ESP_ERR_OPENROUTER_FUNCTION_NOT_FOUND (ESP_ERR_OPENROUTER_BASE + 6)
#define ESP_ERR_OPENROUTER_TOOLS_DISABLED (ESP_ERR_OPENROUTER_BASE + 7)
#define ESP_ERR_OPENROUTER_MAX_FUNCTIONS (ESP_ERR_OPENROUTER_BASE + 8)
```

| Error Code | Description | Common Causes |
|------------|-------------|---------------|
| `ESP_ERR_OPENROUTER_MEMORY` | Memory allocation failure | Large responses, insufficient heap |
| `ESP_ERR_OPENROUTER_JSON_PARSE` | JSON parsing error | Malformed API response |
| `ESP_ERR_OPENROUTER_API_ERROR` | OpenRouter API error | Invalid API key, quota exceeded |
| `ESP_ERR_OPENROUTER_HTTP_ERROR` | HTTP communication error | Network connectivity issues |
| `ESP_ERR_OPENROUTER_FUNCTION_EXISTS` | Function already registered | Duplicate function names |
| `ESP_ERR_OPENROUTER_FUNCTION_NOT_FOUND` | Function not found | Invalid function name |
| `ESP_ERR_OPENROUTER_TOOLS_DISABLED` | Tools/functions not enabled | Configuration issue |
| `ESP_ERR_OPENROUTER_MAX_FUNCTIONS` | Too many functions registered | Exceeded function limit |

---

## Error Handling Patterns

### Basic Error Checking

```c
esp_err_t result = openrouter_call(handle, prompt, response, sizeof(response));
if (result != ESP_OK) {
    ESP_LOGE(TAG, "API call failed: %s", esp_err_to_name(result));
    return result;
}
```

### Comprehensive Error Handler

```c
typedef enum {
    ERROR_ACTION_RETRY,
    ERROR_ACTION_ABORT,
    ERROR_ACTION_FALLBACK
} error_action_t;

error_action_t analyze_error(esp_err_t err) {
    switch (err) {
        // Retry-able errors
        case ESP_ERR_TIMEOUT:
        case ESP_ERR_OPENROUTER_HTTP_ERROR:
            ESP_LOGW(TAG, "Temporary error, will retry: %s", esp_err_to_name(err));
            return ERROR_ACTION_RETRY;
            
        // Fatal errors
        case ESP_ERR_INVALID_ARG:
        case ESP_ERR_OPENROUTER_TOOLS_DISABLED:
        case ESP_ERR_OPENROUTER_FUNCTION_NOT_FOUND:
            ESP_LOGE(TAG, "Fatal error, aborting: %s", esp_err_to_name(err));
            return ERROR_ACTION_ABORT;
            
        // Memory issues - try fallback
        case ESP_ERR_NO_MEM:
        case ESP_ERR_OPENROUTER_MEMORY:
            ESP_LOGW(TAG, "Memory issue, trying fallback: %s", esp_err_to_name(err));
            return ERROR_ACTION_FALLBACK;
            
        default:
            ESP_LOGE(TAG, "Unknown error: %s", esp_err_to_name(err));
            return ERROR_ACTION_ABORT;
    }
}

esp_err_t robust_api_call(openrouter_handle_t handle, const char *prompt, 
                         char *response, size_t response_size) {
    const int max_retries = 3;
    const int retry_delay_ms = 1000;
    
    for (int attempt = 0; attempt < max_retries; attempt++) {
        esp_err_t err = openrouter_call(handle, prompt, response, response_size);
        
        if (err == ESP_OK) {
            return ESP_OK;
        }
        
        error_action_t action = analyze_error(err);
        
        switch (action) {
            case ERROR_ACTION_RETRY:
                if (attempt < max_retries - 1) {
                    ESP_LOGI(TAG, "Retrying in %d ms (attempt %d/%d)", 
                             retry_delay_ms, attempt + 1, max_retries);
                    vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
                    continue;
                }
                break;
                
            case ERROR_ACTION_FALLBACK:
                return try_fallback_approach(handle, prompt, response, response_size);
                
            case ERROR_ACTION_ABORT:
            default:
                return err;
        }
    }
    
    ESP_LOGE(TAG, "All retry attempts failed");
    return ESP_FAIL;
}
```

### Context-Preserving Error Handler

```c
typedef struct {
    esp_err_t last_error;
    char error_message[256];
    uint32_t error_count;
    uint32_t last_error_time;
} error_context_t;

void record_error(error_context_t *ctx, esp_err_t err, const char *operation) {
    ctx->last_error = err;
    ctx->error_count++;
    ctx->last_error_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    snprintf(ctx->error_message, sizeof(ctx->error_message),
             "Operation '%s' failed: %s", operation, esp_err_to_name(err));
    
    ESP_LOGE(TAG, "%s (total errors: %lu)", ctx->error_message, ctx->error_count);
}

esp_err_t monitored_api_call(openrouter_handle_t handle, const char *prompt,
                            char *response, size_t response_size,
                            error_context_t *error_ctx) {
    esp_err_t err = openrouter_call(handle, prompt, response, response_size);
    
    if (err != ESP_OK) {
        record_error(error_ctx, err, "openrouter_call");
        
        // Check if we're having persistent issues
        if (error_ctx->error_count > 5) {
            ESP_LOGE(TAG, "Multiple consecutive errors detected, system may need attention");
        }
    }
    
    return err;
}
```

---

## Debugging Techniques

### 1. Enable Verbose Logging

Configure verbose logging in menuconfig:

```bash
idf.py menuconfig
# Component config → OpenRouter Client → Enable verbose logging
```

Or programmatically:

```c
// Enable debug logging for OpenRouter
esp_log_level_set("OPENROUTER", ESP_LOG_DEBUG);

// Enable verbose HTTP client logging
esp_log_level_set("HTTP_CLIENT", ESP_LOG_DEBUG);
```

### 2. Response Content Debugging

```c
esp_err_t debug_api_call(openrouter_handle_t handle, const char *prompt) {
    char response[4096];
    
    ESP_LOGI(TAG, "Making API call with prompt: '%s'", prompt);
    
    esp_err_t err = openrouter_call(handle, prompt, response, sizeof(response));
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "API call succeeded");
        ESP_LOGI(TAG, "Response length: %d characters", strlen(response));
        ESP_LOGI(TAG, "Response content: %s", response);
    } else {
        ESP_LOGE(TAG, "API call failed: %s", esp_err_to_name(err));
        
        // Check if response contains any partial data
        if (strlen(response) > 0) {
            ESP_LOGW(TAG, "Partial response received: %s", response);
        }
    }
    
    return err;
}
```

### 3. Memory Usage Debugging

```c
void log_memory_usage(const char *operation) {
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    
    ESP_LOGI(TAG, "%s - Free heap: %zu bytes, Min free: %zu bytes", 
             operation, free_heap, min_free_heap);
    
    if (free_heap < 10240) { // Less than 10KB
        ESP_LOGW(TAG, "Low memory warning! Free heap: %zu bytes", free_heap);
    }
}

esp_err_t memory_aware_api_call(openrouter_handle_t handle, const char *prompt,
                               char *response, size_t response_size) {
    log_memory_usage("Before API call");
    
    esp_err_t err = openrouter_call(handle, prompt, response, response_size);
    
    log_memory_usage("After API call");
    
    return err;
}
```

### 4. Network Connectivity Testing

```c
esp_err_t test_network_connectivity(void) {
    esp_err_t err;
    
    // Test basic network connectivity
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr("8.8.8.8"); // Google DNS
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(53);
    
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }
    
    err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    close(sock);
    
    if (err != 0) {
        ESP_LOGE(TAG, "Socket connect failed: errno %d", errno);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Network connectivity test passed");
    return ESP_OK;
}

esp_err_t api_call_with_network_check(openrouter_handle_t handle, const char *prompt,
                                     char *response, size_t response_size) {
    // Check network first
    esp_err_t err = test_network_connectivity();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Network connectivity test failed, skipping API call");
        return err;
    }
    
    return openrouter_call(handle, prompt, response, response_size);
}
```

---

## Recovery Strategies

### 1. Automatic Retry with Exponential Backoff

```c
esp_err_t api_call_with_backoff(openrouter_handle_t handle, const char *prompt,
                               char *response, size_t response_size) {
    const int max_retries = 5;
    int delay_ms = 1000; // Start with 1 second
    
    for (int attempt = 0; attempt < max_retries; attempt++) {
        esp_err_t err = openrouter_call(handle, prompt, response, response_size);
        
        if (err == ESP_OK) {
            if (attempt > 0) {
                ESP_LOGI(TAG, "API call succeeded on attempt %d", attempt + 1);
            }
            return ESP_OK;
        }
        
        // Don't retry for certain errors
        if (err == ESP_ERR_INVALID_ARG || err == ESP_ERR_OPENROUTER_TOOLS_DISABLED) {
            ESP_LOGE(TAG, "Non-retryable error: %s", esp_err_to_name(err));
            return err;
        }
        
        if (attempt < max_retries - 1) {
            ESP_LOGW(TAG, "Attempt %d failed: %s, retrying in %d ms", 
                     attempt + 1, esp_err_to_name(err), delay_ms);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            delay_ms *= 2; // Exponential backoff
            if (delay_ms > 30000) delay_ms = 30000; // Cap at 30 seconds
        }
    }
    
    ESP_LOGE(TAG, "All %d attempts failed", max_retries);
    return ESP_FAIL;
}
```

### 2. Fallback Strategies

```c
esp_err_t try_fallback_approach(openrouter_handle_t handle, const char *prompt,
                               char *response, size_t response_size) {
    ESP_LOGW(TAG, "Trying fallback approach");
    
    // Strategy 1: Try with smaller response size
    if (response_size > 1024) {
        ESP_LOGI(TAG, "Fallback: Reducing response size from %zu to 1024", response_size);
        esp_err_t err = openrouter_call(handle, prompt, response, 1024);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Fallback with smaller buffer succeeded");
            return ESP_OK;
        }
    }
    
    // Strategy 2: Try with different model
    char original_model[128];
    // Save original model (implementation depends on your design)
    
    ESP_LOGI(TAG, "Fallback: Trying with different model");
    esp_err_t err = openrouter_set_model(handle, "openai/gpt-3.5-turbo");
    if (err == ESP_OK) {
        err = openrouter_call(handle, prompt, response, response_size);
        // Restore original model
        openrouter_set_model(handle, original_model);
        
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Fallback with different model succeeded");
            return ESP_OK;
        }
    }
    
    // Strategy 3: Try simplified prompt
    ESP_LOGI(TAG, "Fallback: Trying with simplified prompt");
    char simple_prompt[256];
    snprintf(simple_prompt, sizeof(simple_prompt), "Briefly: %.*s", 200, prompt);
    
    err = openrouter_call(handle, simple_prompt, response, response_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Fallback with simplified prompt succeeded");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "All fallback strategies failed");
    return ESP_FAIL;
}
```

### 3. Circuit Breaker Pattern

```c
typedef enum {
    CIRCUIT_CLOSED,    // Normal operation
    CIRCUIT_OPEN,      // Failing, reject requests
    CIRCUIT_HALF_OPEN  // Testing if service recovered
} circuit_state_t;

typedef struct {
    circuit_state_t state;
    uint32_t failure_count;
    uint32_t failure_threshold;
    uint32_t success_threshold;
    uint32_t timeout_ms;
    uint32_t last_failure_time;
} circuit_breaker_t;

esp_err_t circuit_breaker_call(circuit_breaker_t *cb, openrouter_handle_t handle,
                              const char *prompt, char *response, size_t response_size) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    switch (cb->state) {
        case CIRCUIT_OPEN:
            if (current_time - cb->last_failure_time > cb->timeout_ms) {
                ESP_LOGI(TAG, "Circuit breaker transitioning to half-open");
                cb->state = CIRCUIT_HALF_OPEN;
            } else {
                ESP_LOGW(TAG, "Circuit breaker is open, rejecting request");
                return ESP_ERR_TIMEOUT;
            }
            break;
            
        case CIRCUIT_HALF_OPEN:
        case CIRCUIT_CLOSED:
            break;
    }
    
    esp_err_t err = openrouter_call(handle, prompt, response, response_size);
    
    if (err == ESP_OK) {
        if (cb->state == CIRCUIT_HALF_OPEN) {
            ESP_LOGI(TAG, "Circuit breaker closing after successful call");
            cb->state = CIRCUIT_CLOSED;
            cb->failure_count = 0;
        }
    } else {
        cb->failure_count++;
        cb->last_failure_time = current_time;
        
        if (cb->failure_count >= cb->failure_threshold) {
            ESP_LOGW(TAG, "Circuit breaker opening after %lu failures", cb->failure_count);
            cb->state = CIRCUIT_OPEN;
        }
    }
    
    return err;
}
```

---

## Common Issues

### Issue 1: API Key Problems

**Symptoms:**
- HTTP 401 errors
- "Invalid API key" messages
- Authentication failures

**Debugging:**
```c
esp_err_t validate_api_key(const char *api_key) {
    if (!api_key) {
        ESP_LOGE(TAG, "API key is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t key_len = strlen(api_key);
    if (key_len == 0) {
        ESP_LOGE(TAG, "API key is empty");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (key_len < 20) {
        ESP_LOGE(TAG, "API key seems too short: %zu characters", key_len);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strncmp(api_key, "sk-or-v1-", 9) != 0) {
        ESP_LOGW(TAG, "API key doesn't start with expected prefix");
    }
    
    ESP_LOGI(TAG, "API key validation passed (%zu characters)", key_len);
    return ESP_OK;
}
```

### Issue 2: Memory Exhaustion

**Symptoms:**
- ESP_ERR_NO_MEM errors
- System crashes
- Heap corruption

**Debugging:**
```c
void monitor_heap_usage(const char *context) {
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    
    ESP_LOGI(TAG, "[%s] Heap - Free: %zu, Min: %zu, Largest block: %zu", 
             context, free_heap, min_free, largest_block);
    
    if (free_heap < 20480) { // Less than 20KB
        ESP_LOGW(TAG, "Low memory warning!");
        
        // Force garbage collection if using any managed languages
        // heap_caps_check_integrity_all(true);
    }
}

esp_err_t memory_safe_api_call(openrouter_handle_t handle, const char *prompt,
                              char *response, size_t response_size) {
    monitor_heap_usage("Before API call");
    
    // Check if we have enough memory for the operation
    size_t estimated_usage = response_size + 2048; // Buffer + overhead
    if (esp_get_free_heap_size() < estimated_usage) {
        ESP_LOGE(TAG, "Insufficient memory for API call (need %zu, have %zu)",
                 estimated_usage, esp_get_free_heap_size());
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t err = openrouter_call(handle, prompt, response, response_size);
    
    monitor_heap_usage("After API call");
    return err;
}
```

### Issue 3: Network Connectivity

**Symptoms:**
- Connection timeouts
- HTTP connection failures
- Intermittent failures

**Debugging:**
```c
esp_err_t diagnose_network_issue(void) {
    esp_err_t err;
    
    // Check WiFi connection
    wifi_ap_record_t ap_info;
    err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Not connected to WiFi: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "Connected to WiFi: %s (RSSI: %d)", ap_info.ssid, ap_info.rssi);
    
    // Check IP address
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    err = esp_netif_get_ip_info(netif, &ip_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get IP info: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
    
    return ESP_OK;
}
```

---

## Troubleshooting Guide

### Quick Diagnostic Checklist

```c
esp_err_t run_diagnostics(openrouter_handle_t handle) {
    ESP_LOGI(TAG, "=== OpenRouter Diagnostics ===");
    
    // 1. Check handle validity
    if (!handle) {
        ESP_LOGE(TAG, "❌ Handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "✅ Handle is valid");
    
    // 2. Check network connectivity
    esp_err_t err = diagnose_network_issue();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Network diagnostics failed");
        return err;
    }
    ESP_LOGI(TAG, "✅ Network connectivity OK");
    
    // 3. Check memory availability
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 50000) { // Less than 50KB
        ESP_LOGW(TAG, "⚠️  Low memory: %zu bytes", free_heap);
    } else {
        ESP_LOGI(TAG, "✅ Memory OK: %zu bytes free", free_heap);
    }
    
    // 4. Test simple API call
    char test_response[512];
    err = openrouter_call(handle, "Say 'test'", test_response, sizeof(test_response));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Test API call failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "✅ Test API call succeeded: %s", test_response);
    
    ESP_LOGI(TAG, "=== Diagnostics Complete ===");
    return ESP_OK;
}
```

### Common Error Solutions

| Error | Likely Cause | Solution |
|-------|--------------|----------|
| `ESP_ERR_INVALID_ARG` | NULL pointer or invalid parameter | Check all function parameters |
| `ESP_ERR_NO_MEM` | Insufficient memory | Reduce buffer sizes, check for leaks |
| `ESP_ERR_TIMEOUT` | Network timeout | Check connectivity, increase timeout |
| `ESP_ERR_OPENROUTER_API_ERROR` | Invalid API key or quota | Verify API key, check account status |
| `ESP_ERR_OPENROUTER_JSON_PARSE` | Malformed response | Enable verbose logging, check response |

---

## Best Practices

### 1. Defensive Programming

```c
esp_err_t safe_api_call(openrouter_handle_t handle, const char *prompt,
                       char *response, size_t response_size) {
    // Validate inputs
    if (!handle) {
        ESP_LOGE(TAG, "Handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!prompt || strlen(prompt) == 0) {
        ESP_LOGE(TAG, "Prompt is empty or NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!response || response_size < 64) {
        ESP_LOGE(TAG, "Response buffer is invalid or too small");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Clear response buffer
    memset(response, 0, response_size);
    
    // Make the call
    return openrouter_call(handle, prompt, response, response_size);
}
```

### 2. Graceful Degradation

```c
esp_err_t resilient_ai_query(openrouter_handle_t handle, const char *prompt,
                            char *response, size_t response_size) {
    // Try full-featured call first
    esp_err_t err = openrouter_call(handle, prompt, response, response_size);
    if (err == ESP_OK) {
        return ESP_OK;
    }
    
    ESP_LOGW(TAG, "Primary AI call failed: %s", esp_err_to_name(err));
    
    // Fallback 1: Try with smaller model
    openrouter_set_model(handle, "openai/gpt-3.5-turbo");
    err = openrouter_call(handle, prompt, response, response_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Fallback to smaller model succeeded");
        return ESP_OK;
    }
    
    // Fallback 2: Provide static response
    ESP_LOGW(TAG, "All AI calls failed, using static response");
    snprintf(response, response_size, 
             "Sorry, I'm temporarily unavailable. Please try again later.");
    
    return ESP_OK; // Consider this a successful graceful degradation
}
```

### 3. Monitoring and Alerting

```c
typedef struct {
    uint32_t total_calls;
    uint32_t successful_calls;
    uint32_t failed_calls;
    uint32_t last_success_time;
    esp_err_t last_error;
} api_stats_t;

static api_stats_t g_stats = {0};

esp_err_t monitored_api_call(openrouter_handle_t handle, const char *prompt,
                            char *response, size_t response_size) {
    g_stats.total_calls++;
    
    esp_err_t err = openrouter_call(handle, prompt, response, response_size);
    
    if (err == ESP_OK) {
        g_stats.successful_calls++;
        g_stats.last_success_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    } else {
        g_stats.failed_calls++;
        g_stats.last_error = err;
        
        // Alert if failure rate is high
        if (g_stats.total_calls > 10) {
            float failure_rate = (float)g_stats.failed_calls / g_stats.total_calls;
            if (failure_rate > 0.5f) {
                ESP_LOGW(TAG, "High failure rate detected: %.1f%%", failure_rate * 100);
            }
        }
    }
    
    // Periodic stats logging
    if (g_stats.total_calls % 50 == 0) {
        ESP_LOGI(TAG, "API Stats - Total: %lu, Success: %lu, Failed: %lu",
                 g_stats.total_calls, g_stats.successful_calls, g_stats.failed_calls);
    }
    
    return err;
}
```

---

*For more debugging information, see the [Configuration Guide](configuration.md) for logging options and the [examples directory](../examples/) for working code samples.*
