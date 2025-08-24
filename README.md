# OpenRouter ESP-IDF Client

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.0%2B-blue)](https://idf.espressif.com/)
[![Version](https://img.shields.io/badge/Version-1.0.1-green)](https://github.com/nikhil-robinson/openrouter_client)

A comprehensive OpenRouter API client library for ESP32 microcontrollers using ESP-IDF. This library enables seamless integration with OpenRouter's AI models, supporting text generation, streaming responses, function calling, and multimodal capabilities (image and audio processing).

## ✨ Features

### Core Capabilities
- 🚀 **Full OpenRouter API Support** - Complete integration with OpenRouter's chat completions API
- 📡 **Streaming & Non-Streaming** - Real-time token-by-token streaming or complete responses
- 🔧 **Function Calling (Tools)** - Register custom functions that AI models can invoke
- 🖼️ **Multimodal Support** - Process images and audio files alongside text
- ⚙️ **Highly Configurable** - Extensive configuration options via ESP-IDF menuconfig
- 🔒 **Secure** - Built-in TLS/SSL support with certificate validation
- 💾 **Memory Efficient** - Optimized for ESP32's memory constraints

### Advanced Features
- **Dynamic Buffer Management** - Automatic buffer resizing for large responses
- **Connection Pooling** - HTTP connection reuse for improved performance
- **Error Handling** - Comprehensive error reporting and recovery
- **Flexible Authentication** - Support for API keys and custom headers
- **Model Parameters** - Full control over temperature, top-p, max tokens, and more

## 📋 Requirements

- **ESP-IDF**: Version 5.0 or higher
- **Hardware**: ESP32, ESP32-S2, ESP32-S3, or ESP32-C3
- **Memory**: Minimum 4MB flash, 320KB RAM recommended
- **Network**: Wi-Fi connectivity with internet access
- **API Key**: Valid OpenRouter API key from [openrouter.ai](https://openrouter.ai)

## 🚀 Quick Start

### 1. Installation

Add this component to your ESP-IDF project using the component manager:

```yaml
# idf_component.yml
dependencies:
  openrouter_client:
    git: https://github.com/nikhil-robinson/openrouter_client.git
    version: "^1.0.0"
```

Or clone directly into your components directory:

```bash
cd components
git clone https://github.com/nikhil-robinson/openrouter_client.git
```

### 2. Basic Configuration

Configure the component via menuconfig:

```bash
idf.py menuconfig
```

Navigate to `Component config → OpenRouter Client` and set:
- Default response buffer size (recommended: 4096)
- Model temperature (0.0-2.0, default: 0.7)
- Maximum tokens (default: 1024)
- HTTP timeouts

### 3. Simple Usage Example

```c
#include "openrouter.h"
#include "esp_log.h"

static const char *TAG = "OPENROUTER_EXAMPLE";

void app_main(void) {
    // Initialize Wi-Fi (implementation not shown)
    // wifi_init_sta();
    
    // Configure OpenRouter client
    openrouter_config_t config = {
        .api_key = "your_openrouter_api_key",
        .default_model = "openai/gpt-3.5-turbo",
        .default_system_role = "You are a helpful assistant.",
        .temperature = 0.7f,
        .max_tokens = 1024,
        .response_buffer_size = 4096,
        .enable_streaming = false
    };
    
    // Create client handle
    openrouter_handle_t handle = openrouter_create(&config);
    if (!handle) {
        ESP_LOGE(TAG, "Failed to create OpenRouter handle");
        return;
    }
    
    // Make API call
    char response[4096];
    esp_err_t err = openrouter_call(handle, "Tell me a joke about ESP32", 
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

## 📖 API Reference

### Core Functions

#### Client Management
```c
openrouter_handle_t openrouter_create(const openrouter_config_t *config);
void openrouter_destroy(openrouter_handle_t handle);
```

#### Basic API Calls
```c
esp_err_t openrouter_call(openrouter_handle_t handle, const char *prompt, 
                         char *response, size_t response_size);

esp_err_t openrouter_call_with_system(openrouter_handle_t handle, 
                                     const char *system_role, const char *prompt,
                                     char *response, size_t response_size);
```

#### Streaming API
```c
esp_err_t openrouter_stream_call(openrouter_handle_t handle, const char *prompt,
                                openrouter_stream_callback_t callback, void *user_data);

// Streaming callback type
typedef void (*openrouter_stream_callback_t)(const char *content, bool is_complete, void *user_data);
```

### Configuration Structure

```c
typedef struct {
    const char *api_key;                    // Required: OpenRouter API key
    const char *default_model;              // AI model to use
    const char *default_system_role;        // System role/instructions
    float temperature;                      // Response randomness (0.0-2.0)
    int max_tokens;                        // Maximum response tokens
    float top_p;                           // Top-p sampling parameter
    int seed;                              // Random seed (-1 for random)
    size_t response_buffer_size;           // Internal buffer size
    bool enable_streaming;                 // Enable streaming mode
    bool enable_tools;                     // Enable function calling
    int http_timeout;                      // HTTP timeout (ms)
    bool connection_reuse;                 // Enable connection pooling
} openrouter_config_t;
```

## 🔧 Advanced Features

### Function Calling (Tools)

Register custom functions that AI models can invoke:

```c
// Define function parameters
static const openrouter_param_t weather_params[] = {
    {"location", "string", "City and state, e.g. San Francisco, CA", true, NULL},
    {"unit", "string", "Temperature unit", false, (const char*[]){"celsius", "fahrenheit", NULL}},
    {NULL, NULL, NULL, false, NULL}  // Terminator
};

// Implement function callback
char* get_weather(const char *function_name, const char *arguments, void *user_data) {
    // Parse arguments, get weather data, return JSON response
    return strdup("{\"temperature\": 22, \"unit\": \"celsius\", \"description\": \"sunny\"}");
}

// Register function
openrouter_simple_function_t weather_func = {
    .name = "get_weather",
    .description = "Get current weather for a location",
    .parameters = weather_params,
    .callback = get_weather,
    .user_data = NULL
};

openrouter_register_simple_function(handle, &weather_func);
```

### Multimodal Support

Process images and audio alongside text:

```c
// Add image from file
openrouter_add_image_file(handle, "/spiffs/image.jpg", "Describe this image");

// Add image from URL
openrouter_add_image_url(handle, "https://example.com/image.jpg", "What's in this picture?");

// Add audio file
openrouter_add_audio_file(handle, "/spiffs/audio.mp3", "Transcribe this audio");

// Make multimodal call
char response[4096];
esp_err_t err = openrouter_call(handle, "Analyze the provided media", response, sizeof(response));
```

### Streaming with Function Calls

Combine streaming responses with function calling:

```c
void stream_callback(const char *content, bool is_complete, void *user_data) {
    if (is_complete) {
        printf("\n[Stream Complete]\n");
    } else {
        printf("%s", content);
        fflush(stdout);
    }
}

// Enable streaming and tools
openrouter_config_t config = {
    .api_key = "your_api_key",
    .default_model = "openai/gpt-4",
    .enable_streaming = true,
    .enable_tools = true,
    // ... other config
};

// Make streaming call with tools
openrouter_stream_call_with_tools(handle, "What's the weather like?", 
                                 stream_callback, NULL);
```

## 📊 Supported Models

The library supports all OpenRouter models, including:

### Text Models
- **OpenAI**: GPT-4, GPT-4 Turbo, GPT-3.5 Turbo
- **Anthropic**: Claude 3.5 Sonnet, Claude 3 Haiku
- **Google**: Gemini Pro, Gemini Flash
- **Meta**: Llama 3.1, Llama 2
- **Mistral**: Mistral Large, Mistral 7B
- **DeepSeek**: DeepSeek Chat, DeepSeek Coder

### Multimodal Models
- **OpenAI**: GPT-4 Vision, GPT-4o
- **Google**: Gemini Pro Vision
- **Anthropic**: Claude 3 with vision

See [OpenRouter Models](https://openrouter.ai/models) for the complete list.

## 📁 Examples

The `examples/` directory contains comprehensive demonstrations:

### Basic Examples
- **[openrouter_text_model](examples/openrouter_text_model/)** - Simple non-streaming text generation
- **[openrouter_text_model_streaming](examples/openrouter_text_model_streaming/)** - Real-time streaming responses

### Advanced Examples
- **[function_calling_example](examples/function_calling_example/)** - Custom function registration and calling
- **[multimodal_example](examples/multimodal_example/)** - Image and audio processing

To run an example:

```bash
cd examples/openrouter_text_model
idf.py menuconfig  # Configure Wi-Fi and API key
idf.py build flash monitor
```

## ⚙️ Configuration

### Menuconfig Options

Access via `idf.py menuconfig → Component config → OpenRouter Client`:

| Option | Default | Description |
|--------|---------|-------------|
| Response Buffer Size | 4096 | Default buffer size for responses |
| HTTP Timeout | 30000 | HTTP request timeout (ms) |
| Streaming Timeout | 60000 | Streaming request timeout (ms) |
| Temperature | 0.7 | Default model temperature |
| Max Tokens | 1024 | Default maximum tokens |
| Top-P | 1.0 | Default top-p sampling |
| Verbose Logging | Off | Enable detailed debug logs |
| Connection Reuse | Off | Enable HTTP connection pooling |

### Runtime Configuration

Many parameters can be configured at runtime:

```c
// Set model parameters
openrouter_set_temperature(handle, 0.9f);
openrouter_set_max_tokens(handle, 2048);
openrouter_set_model(handle, "anthropic/claude-3-sonnet");

// Enable/disable features
openrouter_set_streaming(handle, true);
openrouter_set_tools_enabled(handle, true);
```

## 🔧 Troubleshooting

### Common Issues

**Wi-Fi Connection Problems**
```
E (1234) OPENROUTER: HTTP request failed: ESP_ERR_ESP_TLS_TCP_CLOSED_ON_INVALID_CERT
```
- Ensure correct time/date (required for TLS)
- Check Wi-Fi credentials
- Verify internet connectivity

**Memory Issues**
```
E (1234) OPENROUTER: Failed to allocate response buffer
```
- Reduce `response_buffer_size` in configuration
- Increase available heap memory
- Use streaming for large responses

**API Errors**
```
E (1234) OPENROUTER: HTTP 401: Invalid API key
```
- Verify OpenRouter API key
- Check API key permissions
- Ensure sufficient credits

### Debug Logging

Enable verbose logging for detailed diagnostics:

```bash
idf.py menuconfig
# Component config → OpenRouter Client → Enable verbose logging
```

Or programmatically:
```c
esp_log_level_set("OPENROUTER", ESP_LOG_DEBUG);
```

## 🤝 Contributing

We welcome contributions! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

### Development Setup

```bash
git clone https://github.com/nikhil-robinson/openrouter_client.git
cd openrouter_client
# Make your changes
# Test with examples
```

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- [ESP-IDF](https://github.com/espressif/esp-idf) by Espressif Systems
- [cJSON](https://github.com/DaveGamble/cJSON) for JSON parsing
- [OpenRouter](https://openrouter.ai) for AI API access

## 📞 Support

- 🐛 **Issues**: [GitHub Issues](https://github.com/nikhil-robinson/openrouter_client/issues)
- 📖 **Documentation**: [OpenRouter API Docs](https://openrouter.ai/docs)
- 💬 **Community**: [ESP32 Forums](https://esp32.com/)

---

<p align="center">
  <strong>Built with ❤️ for the ESP32 community</strong>
</p>
