# Streaming Guide

This guide covers real-time streaming responses with the OpenRouter ESP-IDF library, enabling token-by-token delivery for responsive user interfaces.

## Table of Contents

- [What is Streaming?](#what-is-streaming)
- [Basic Streaming Setup](#basic-streaming-setup)
- [Streaming Callbacks](#streaming-callbacks)
- [Advanced Streaming Patterns](#advanced-streaming-patterns)
- [Performance Optimization](#performance-optimization)
- [Error Handling in Streaming](#error-handling-in-streaming)
- [Best Practices](#best-practices)
- [Common Use Cases](#common-use-cases)

---

## What is Streaming?

Streaming allows you to receive AI responses token-by-token as they're generated, rather than waiting for the complete response. This provides several benefits:

- **Immediate Feedback**: Users see output start appearing quickly
- **Better UX**: Perception of faster response times
- **Memory Efficiency**: Process tokens incrementally
- **Early Termination**: Stop processing if needed

### Streaming vs Non-Streaming

```c
// Non-streaming: Wait for complete response
char response[4096];
esp_err_t err = openrouter_call(handle, "Explain ESP32", response, sizeof(response));
// User waits 5-10 seconds for complete response

// Streaming: Process tokens as they arrive
esp_err_t err = openrouter_call_streaming(handle, "Explain ESP32", stream_callback, NULL);
// User sees response start appearing in ~1 second
```

---

## Basic Streaming Setup

### 1. Enable Streaming in Configuration

```c
openrouter_config_t config = {
    .api_key = "your_api_key",
    .default_model = "openai/gpt-3.5-turbo",
    .temperature = 0.7f,
    .max_tokens = 1024,
    .enable_streaming = true,  // Enable streaming mode
    .response_buffer_size = 4096
};
```

### 2. Implement a Stream Callback

```c
void simple_stream_callback(const char *content, bool is_complete, void *user_data) {
    if (is_complete) {
        printf("\n[Response Complete]\n");
    } else {
        printf("%s", content);  // Print each token as it arrives
        fflush(stdout);         // Ensure immediate output
    }
}
```

### 3. Make a Streaming Call

```c
esp_err_t make_streaming_call(openrouter_handle_t handle) {
    esp_err_t err = openrouter_call_streaming(handle,
        "Explain how ESP32 WiFi works",
        simple_stream_callback,
        NULL);  // No user data needed
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Streaming call failed: %s", esp_err_to_name(err));
    }
    
    return err;
}
```

---

## Streaming Callbacks

### Callback Function Signature

```c
typedef void (*openrouter_stream_callback_t)(const char *content, bool is_complete, void *user_data);
```

**Parameters:**
- `content` - Token content string (empty when `is_complete` is true)
- `is_complete` - True for the final callback, false for token callbacks
- `user_data` - User-provided context data

### Basic Callback Examples

#### Console Output Callback
```c
void console_stream_callback(const char *content, bool is_complete, void *user_data) {
    if (is_complete) {
        printf("\n--- End of Response ---\n");
    } else {
        printf("%s", content);
        fflush(stdout);
    }
}
```

#### Buffer Accumulation Callback
```c
typedef struct {
    char *buffer;
    size_t buffer_size;
    size_t current_pos;
    bool completed;
} stream_buffer_t;

void buffer_stream_callback(const char *content, bool is_complete, void *user_data) {
    stream_buffer_t *buf = (stream_buffer_t*)user_data;
    
    if (is_complete) {
        buf->completed = true;
        ESP_LOGI(TAG, "Streaming complete. Total length: %zu", buf->current_pos);
        return;
    }
    
    // Append content to buffer
    size_t content_len = strlen(content);
    if (buf->current_pos + content_len < buf->buffer_size - 1) {
        strcpy(buf->buffer + buf->current_pos, content);
        buf->current_pos += content_len;
        buf->buffer[buf->current_pos] = '\0';
    } else {
        ESP_LOGW(TAG, "Buffer full, truncating content");
    }
}

// Usage
esp_err_t stream_to_buffer(openrouter_handle_t handle, const char *prompt, 
                          char *buffer, size_t buffer_size) {
    stream_buffer_t stream_buf = {
        .buffer = buffer,
        .buffer_size = buffer_size,
        .current_pos = 0,
        .completed = false
    };
    
    buffer[0] = '\0';  // Initialize buffer
    
    esp_err_t err = openrouter_call_streaming(handle, prompt, 
                                             buffer_stream_callback, &stream_buf);
    
    // Wait for completion (in real apps, you might do this differently)
    while (!stream_buf.completed && err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    return err;
}
```

#### Event-Driven Callback
```c
typedef struct {
    TaskHandle_t waiting_task;
    char *response_buffer;
    size_t buffer_size;
    bool done;
} stream_context_t;

void event_stream_callback(const char *content, bool is_complete, void *user_data) {
    stream_context_t *ctx = (stream_context_t*)user_data;
    
    if (is_complete) {
        ctx->done = true;
        // Notify waiting task
        if (ctx->waiting_task) {
            xTaskNotifyGive(ctx->waiting_task);
        }
    } else {
        // Process token (could send to queue, update display, etc.)
        size_t current_len = strlen(ctx->response_buffer);
        size_t content_len = strlen(content);
        
        if (current_len + content_len < ctx->buffer_size - 1) {
            strcat(ctx->response_buffer, content);
        }
    }
}
```

---

## Advanced Streaming Patterns

### 1. Line-by-Line Processing

```c
typedef struct {
    char line_buffer[256];
    size_t line_pos;
    void (*line_callback)(const char *line, void *user_data);
    void *line_user_data;
} line_processor_t;

void line_stream_callback(const char *content, bool is_complete, void *user_data) {
    line_processor_t *processor = (line_processor_t*)user_data;
    
    if (is_complete) {
        // Process final line if any
        if (processor->line_pos > 0) {
            processor->line_buffer[processor->line_pos] = '\0';
            processor->line_callback(processor->line_buffer, processor->line_user_data);
        }
        return;
    }
    
    // Process character by character
    for (const char *p = content; *p; p++) {
        if (*p == '\n' || processor->line_pos >= sizeof(processor->line_buffer) - 1) {
            // Complete line found
            processor->line_buffer[processor->line_pos] = '\0';
            processor->line_callback(processor->line_buffer, processor->line_user_data);
            processor->line_pos = 0;
        } else {
            processor->line_buffer[processor->line_pos++] = *p;
        }
    }
}

void process_line(const char *line, void *user_data) {
    ESP_LOGI(TAG, "Complete line: %s", line);
    // Could send to display, parse for commands, etc.
}

esp_err_t stream_line_by_line(openrouter_handle_t handle, const char *prompt) {
    line_processor_t processor = {
        .line_pos = 0,
        .line_callback = process_line,
        .line_user_data = NULL
    };
    
    return openrouter_call_streaming(handle, prompt, line_stream_callback, &processor);
}
```

### 2. Token Counting and Statistics

```c
typedef struct {
    uint32_t token_count;
    uint32_t word_count;
    uint32_t char_count;
    uint32_t start_time;
    float tokens_per_second;
} stream_stats_t;

void stats_stream_callback(const char *content, bool is_complete, void *user_data) {
    stream_stats_t *stats = (stream_stats_t*)user_data;
    
    if (is_complete) {
        uint32_t elapsed_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - stats->start_time;
        stats->tokens_per_second = (float)stats->token_count / (elapsed_ms / 1000.0f);
        
        ESP_LOGI(TAG, "Streaming stats:");
        ESP_LOGI(TAG, "  Tokens: %lu", stats->token_count);
        ESP_LOGI(TAG, "  Words: %lu", stats->word_count);
        ESP_LOGI(TAG, "  Characters: %lu", stats->char_count);
        ESP_LOGI(TAG, "  Speed: %.2f tokens/sec", stats->tokens_per_second);
        return;
    }
    
    // Count this token
    stats->token_count++;
    
    // Count characters and words
    for (const char *p = content; *p; p++) {
        stats->char_count++;
        if (*p == ' ' || *p == '\t' || *p == '\n') {
            stats->word_count++;
        }
    }
    
    // Print content
    printf("%s", content);
    fflush(stdout);
}

esp_err_t stream_with_stats(openrouter_handle_t handle, const char *prompt) {
    stream_stats_t stats = {
        .token_count = 0,
        .word_count = 0,
        .char_count = 0,
        .start_time = xTaskGetTickCount() * portTICK_PERIOD_MS,
        .tokens_per_second = 0.0f
    };
    
    return openrouter_call_streaming(handle, prompt, stats_stream_callback, &stats);
}
```

### 3. Multi-Output Streaming

```c
typedef struct {
    FILE *file_output;
    char *buffer;
    size_t buffer_size;
    size_t buffer_pos;
    void (*display_callback)(const char *text);
} multi_output_t;

void multi_output_callback(const char *content, bool is_complete, void *user_data) {
    multi_output_t *outputs = (multi_output_t*)user_data;
    
    if (is_complete) {
        // Finalize all outputs
        if (outputs->file_output) {
            fflush(outputs->file_output);
        }
        if (outputs->display_callback) {
            outputs->display_callback("[COMPLETE]");
        }
        return;
    }
    
    // Send to console
    printf("%s", content);
    fflush(stdout);
    
    // Send to file
    if (outputs->file_output) {
        fprintf(outputs->file_output, "%s", content);
    }
    
    // Send to buffer
    if (outputs->buffer && outputs->buffer_pos < outputs->buffer_size - 1) {
        size_t len = strlen(content);
        size_t copy_len = (outputs->buffer_pos + len < outputs->buffer_size - 1) ? 
                          len : (outputs->buffer_size - 1 - outputs->buffer_pos);
        memcpy(outputs->buffer + outputs->buffer_pos, content, copy_len);
        outputs->buffer_pos += copy_len;
        outputs->buffer[outputs->buffer_pos] = '\0';
    }
    
    // Send to display callback
    if (outputs->display_callback) {
        outputs->display_callback(content);
    }
}
```

---

## Performance Optimization

### 1. Efficient String Handling

```c
// Avoid frequent string operations in callbacks
void efficient_stream_callback(const char *content, bool is_complete, void *user_data) {
    if (is_complete) {
        printf("\n");
        return;
    }
    
    // Direct output without string copying
    size_t len = strlen(content);
    fwrite(content, 1, len, stdout);
    fflush(stdout);
}
```

### 2. Batched Processing

```c
typedef struct {
    char batch_buffer[256];
    size_t batch_pos;
    uint32_t last_flush_time;
    uint32_t flush_interval_ms;
} batch_processor_t;

void batched_stream_callback(const char *content, bool is_complete, void *user_data) {
    batch_processor_t *batch = (batch_processor_t*)user_data;
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if (is_complete) {
        // Flush final batch
        if (batch->batch_pos > 0) {
            batch->batch_buffer[batch->batch_pos] = '\0';
            printf("%s\n", batch->batch_buffer);
        }
        return;
    }
    
    // Add to batch
    size_t content_len = strlen(content);
    if (batch->batch_pos + content_len < sizeof(batch->batch_buffer) - 1) {
        strcpy(batch->batch_buffer + batch->batch_pos, content);
        batch->batch_pos += content_len;
    }
    
    // Flush if buffer full or time elapsed
    bool should_flush = (batch->batch_pos >= sizeof(batch->batch_buffer) - 32) ||
                       (current_time - batch->last_flush_time >= batch->flush_interval_ms);
                       
    if (should_flush) {
        batch->batch_buffer[batch->batch_pos] = '\0';
        printf("%s", batch->batch_buffer);
        fflush(stdout);
        batch->batch_pos = 0;
        batch->last_flush_time = current_time;
    }
}
```

### 3. Memory Pool for Callbacks

```c
#define CALLBACK_POOL_SIZE 10
#define CALLBACK_BUFFER_SIZE 512

typedef struct {
    char buffers[CALLBACK_POOL_SIZE][CALLBACK_BUFFER_SIZE];
    bool in_use[CALLBACK_POOL_SIZE];
    SemaphoreHandle_t mutex;
} callback_pool_t;

static callback_pool_t g_callback_pool = {0};

char* get_callback_buffer(callback_pool_t *pool) {
    xSemaphoreTake(pool->mutex, portMAX_DELAY);
    
    for (int i = 0; i < CALLBACK_POOL_SIZE; i++) {
        if (!pool->in_use[i]) {
            pool->in_use[i] = true;
            xSemaphoreGive(pool->mutex);
            return pool->buffers[i];
        }
    }
    
    xSemaphoreGive(pool->mutex);
    return NULL;  // Pool exhausted
}

void return_callback_buffer(callback_pool_t *pool, char *buffer) {
    xSemaphoreTake(pool->mutex, portMAX_DELAY);
    
    for (int i = 0; i < CALLBACK_POOL_SIZE; i++) {
        if (pool->buffers[i] == buffer) {
            pool->in_use[i] = false;
            break;
        }
    }
    
    xSemaphoreGive(pool->mutex);
}
```

---

## Error Handling in Streaming

### Robust Streaming with Error Recovery

```c
typedef struct {
    char *backup_buffer;
    size_t backup_size;
    bool error_occurred;
    esp_err_t last_error;
} resilient_stream_t;

void resilient_stream_callback(const char *content, bool is_complete, void *user_data) {
    resilient_stream_t *ctx = (resilient_stream_t*)user_data;
    
    if (is_complete) {
        if (ctx->error_occurred) {
            ESP_LOGW(TAG, "Streaming completed with errors, using backup");
            printf("%s", ctx->backup_buffer);
        } else {
            printf("\n[Streaming completed successfully]");
        }
        return;
    }
    
    // Try to output content
    if (printf("%s", content) < 0) {
        // Output failed, save to backup
        ctx->error_occurred = true;
        size_t current_len = strlen(ctx->backup_buffer);
        size_t content_len = strlen(content);
        
        if (current_len + content_len < ctx->backup_size - 1) {
            strcat(ctx->backup_buffer, content);
        }
    } else {
        fflush(stdout);
    }
}

esp_err_t resilient_streaming_call(openrouter_handle_t handle, const char *prompt) {
    char backup_buffer[4096] = {0};
    resilient_stream_t ctx = {
        .backup_buffer = backup_buffer,
        .backup_size = sizeof(backup_buffer),
        .error_occurred = false,
        .last_error = ESP_OK
    };
    
    return openrouter_call_streaming(handle, prompt, resilient_stream_callback, &ctx);
}
```

---

## Best Practices

### 1. Keep Callbacks Fast

```c
// Good: Fast callback
void fast_callback(const char *content, bool is_complete, void *user_data) {
    if (is_complete) return;
    
    // Just output directly - fast
    printf("%s", content);
    fflush(stdout);
}

// Bad: Slow callback
void slow_callback(const char *content, bool is_complete, void *user_data) {
    if (is_complete) return;
    
    // Don't do heavy processing in callbacks
    char processed[1024];
    sprintf(processed, "[%lu] %s", xTaskGetTickCount(), content);  // Slow formatting
    
    // Don't do file I/O in callbacks unless necessary
    FILE *f = fopen("/spiffs/log.txt", "a");  // Slow file operation
    fprintf(f, "%s", processed);
    fclose(f);
}
```

### 2. Handle Partial Content Gracefully

```c
typedef struct {
    char partial_buffer[64];
    size_t partial_pos;
} partial_handler_t;

void partial_aware_callback(const char *content, bool is_complete, void *user_data) {
    partial_handler_t *handler = (partial_handler_t*)user_data;
    
    if (is_complete) {
        // Output any remaining partial content
        if (handler->partial_pos > 0) {
            handler->partial_buffer[handler->partial_pos] = '\0';
            printf("%s", handler->partial_buffer);
        }
        printf("\n");
        return;
    }
    
    // Handle content that might be cut off mid-word
    size_t content_len = strlen(content);
    const char *last_space = strrchr(content, ' ');
    
    if (last_space) {
        // Output up to last complete word
        size_t complete_len = last_space - content + 1;
        printf("%.*s", (int)complete_len, content);
        
        // Save partial word for next callback
        const char *partial_start = last_space + 1;
        size_t partial_len = content_len - complete_len;
        if (partial_len < sizeof(handler->partial_buffer) - 1) {
            strcpy(handler->partial_buffer, partial_start);
            handler->partial_pos = partial_len;
        }
    } else {
        // No space found, might be continuation of previous partial
        if (handler->partial_pos > 0) {
            printf("%s", handler->partial_buffer);
            handler->partial_pos = 0;
        }
        printf("%s", content);
    }
    
    fflush(stdout);
}
```

### 3. Use Appropriate Buffer Sizes

```c
// Configure based on expected response patterns
openrouter_config_t configure_for_streaming_type(const char *api_key, const char *response_type) {
    openrouter_config_t config = {
        .api_key = api_key,
        .enable_streaming = true
    };
    
    if (strcmp(response_type, "short_answers") == 0) {
        config.response_buffer_size = 1024;
        config.max_tokens = 256;
        
    } else if (strcmp(response_type, "explanations") == 0) {
        config.response_buffer_size = 4096;
        config.max_tokens = 1024;
        
    } else if (strcmp(response_type, "code_generation") == 0) {
        config.response_buffer_size = 8192;
        config.max_tokens = 2048;
    }
    
    return config;
}
```

---

## Common Use Cases

### 1. Interactive Chat Interface

```c
typedef struct {
    char display_buffer[4096];
    size_t display_pos;
    uint32_t last_update_time;
    bool typing_indicator_shown;
} chat_interface_t;

void chat_stream_callback(const char *content, bool is_complete, void *user_data) {
    chat_interface_t *chat = (chat_interface_t*)user_data;
    
    if (is_complete) {
        chat->typing_indicator_shown = false;
        printf("\n--- AI finished ---\n");
        return;
    }
    
    // Show typing indicator on first token
    if (!chat->typing_indicator_shown) {
        printf("AI is typing");
        chat->typing_indicator_shown = true;
    }
    
    // Update display buffer
    size_t content_len = strlen(content);
    if (chat->display_pos + content_len < sizeof(chat->display_buffer) - 1) {
        strcpy(chat->display_buffer + chat->display_pos, content);
        chat->display_pos += content_len;
    }
    
    // Update display every 100ms to avoid flicker
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (current_time - chat->last_update_time >= 100) {
        printf("\r%s", chat->display_buffer);
        fflush(stdout);
        chat->last_update_time = current_time;
    }
}
```

### 2. Live Code Generation

```c
void code_stream_callback(const char *content, bool is_complete, void *user_data) {
    static bool in_code_block = false;
    static char code_buffer[8192] = {0};
    static size_t code_pos = 0;
    
    if (is_complete) {
        if (in_code_block) {
            printf("\n```\n--- Code generation complete ---\n");
        }
        return;
    }
    
    // Detect code blocks
    if (strstr(content, "```c") || strstr(content, "```")) {
        in_code_block = !in_code_block;
        if (in_code_block) {
            printf("\n--- Generating code ---\n");
            code_pos = 0;
        }
    }
    
    if (in_code_block) {
        // Accumulate code for syntax checking
        size_t content_len = strlen(content);
        if (code_pos + content_len < sizeof(code_buffer) - 1) {
            strcpy(code_buffer + code_pos, content);
            code_pos += content_len;
        }
    }
    
    printf("%s", content);
    fflush(stdout);
}
```

### 3. Real-time Translation

```c
typedef struct {
    char source_buffer[2048];
    size_t source_pos;
    void (*translation_callback)(const char *translated_text);
} translation_context_t;

void translation_stream_callback(const char *content, bool is_complete, void *user_data) {
    translation_context_t *ctx = (translation_context_t*)user_data;
    
    if (is_complete) {
        if (ctx->translation_callback && ctx->source_pos > 0) {
            ctx->source_buffer[ctx->source_pos] = '\0';
            ctx->translation_callback(ctx->source_buffer);
        }
        return;
    }
    
    // Accumulate translation
    size_t content_len = strlen(content);
    if (ctx->source_pos + content_len < sizeof(ctx->source_buffer) - 1) {
        strcpy(ctx->source_buffer + ctx->source_pos, content);
        ctx->source_pos += content_len;
    }
    
    // Also show live progress
    printf("%s", content);
    fflush(stdout);
}
```

---

*For complete working examples of streaming implementations, see the [streaming example](../examples/openrouter_text_model_streaming/) in the examples directory.*
