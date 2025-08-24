# OpenRouter ESP-IDF Client

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.0%2B-blue)](https://idf.espressif.com/)
[![Version](https://img.shields.io/badge/Version-1.0.1-green)](https://github.com/nikhil-robinson/openrouter_client)

Bring the power of modern AI to your ESP32 projects! This library transforms your microcontroller into an intelligent device capable of natural language processing, real-time conversations, and smart decision-making through OpenRouter's extensive model ecosystem.

## ✨ Key Features

- 🚀 **Complete OpenRouter Integration** - Full chat completions API support
- 📡 **Real-time Streaming** - Token-by-token responses for interactive applications  
- 🔧 **AI Function Calling** - Let AI models invoke your custom ESP32 functions
- 🖼️ **Multimodal Processing** - Handle images and audio alongside text
- ⚙️ **ESP32 Optimized** - Memory-efficient design for microcontroller constraints
- 🔒 **Enterprise Security** - TLS/SSL with certificate validation
- � **70+ AI Models** - Access to GPT-4, Claude, Gemini, Llama and more

## 🎯 Quick Start

### Installation

Add to your ESP-IDF project:

```yaml
# idf_component.yml
dependencies:
  openrouter_client:
    git: https://github.com/nikhil-robinson/openrouter_client.git
    version: "^1.0.0"
```

### Basic Example

```c
#include "openrouter.h"

void app_main(void) {
    // Configure client
    openrouter_config_t config = {
        .api_key = "your_openrouter_api_key",
        .default_model = "openai/gpt-3.5-turbo",
        .response_buffer_size = 4096
    };
    
    // Create handle and make API call
    openrouter_handle_t handle = openrouter_create(&config);
    char response[4096];
    
    esp_err_t err = openrouter_call(handle, "Tell me about ESP32", 
                                   response, sizeof(response));
    
    if (err == ESP_OK) {
        printf("AI Response: %s\n", response);
    }
    
    openrouter_destroy(handle);
}
```

> 💡 **New to the library?** Start with our [Basic Usage Guide](docs/basic-usage.md) for a complete walkthrough.

## 📚 Documentation

### 🚀 Getting Started
- **[Basic Usage Guide](docs/basic-usage.md)** - Complete beginner tutorial with examples
- **[Configuration Guide](docs/configuration.md)** - Menuconfig options and runtime configuration  
- **[Error Handling](docs/error-handling.md)** - Troubleshooting and debugging guide

### 🔧 Core Features  
- **[API Reference](docs/api-reference.md)** - Complete function reference
- **[Streaming Guide](docs/streaming.md)** - Real-time token streaming
- **[Function Calling](docs/function-calling.md)** - AI-powered function execution
- **[Data Structures](docs/data-structures.md)** - All structs and type definitions

### 🎯 Use Cases & Examples

| Feature | Example | Documentation |
|---------|---------|---------------|
| **Basic AI Chat** | [`openrouter_text_model`](examples/openrouter_text_model/) | [Basic Usage](docs/basic-usage.md) |
| **Real-time Streaming** | [`openrouter_text_model_streaming`](examples/openrouter_text_model_streaming/) | [Streaming Guide](docs/streaming.md) |
| **Function Calling** | [`function_calling_example`](examples/function_calling_example/) | [Function Calling](docs/function-calling.md) |
| **Image & Audio** | [`multimodal_example`](examples/multimodal_example/) | [Multimodal Guide](docs/api-reference.md#multimodal-api-calls) |

## �️ Requirements

- **ESP-IDF**: v5.0+ (v5.1+ recommended)
- **Hardware**: ESP32/S2/S3/C3 with 4MB+ flash, 320KB+ RAM
- **Network**: Wi-Fi with internet connectivity  
- **API Key**: [OpenRouter account](https://openrouter.ai) required

## 🚀 Supported AI Models

Access 70+ models through OpenRouter's unified API:

**Popular Models:**
- **OpenAI**: GPT-4, GPT-4 Turbo, GPT-3.5 Turbo
- **Anthropic**: Claude 3.5 Sonnet, Claude 3 Haiku  
- **Google**: Gemini Pro, Gemini Flash
- **Meta**: Llama 3.1, Llama 2
- **Mistral**: Mistral Large, Mistral 7B

> 📋 **Full model list**: [OpenRouter Models](https://openrouter.ai/models)

## ⚙️ Configuration

Configure via `idf.py menuconfig → Component config → OpenRouter Client` or see the [Configuration Guide](docs/configuration.md) for complete details.

### Key Settings
- **Response Buffer**: 4096 bytes (adjustable for memory optimization)
- **HTTP Timeout**: 30s (increase for large responses)  
- **Temperature**: 0.7 (response creativity: 0.0-2.0)
- **Max Tokens**: 1024 (response length limit)

## 🛠️ Running Examples

```bash
cd examples/openrouter_text_model
idf.py menuconfig  # Set Wi-Fi credentials and API key
idf.py build flash monitor
```

> 📖 **Need help?** Check our [Error Handling Guide](docs/error-handling.md) for troubleshooting tips.

## 📋 Project Status

- ✅ **Stable**: Core API, streaming, function calling
- 🧪 **Beta**: Multimodal support (images/audio)
- 📈 **Roadmap**: WebSocket streaming, advanced caching

## 🤝 Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch  
3. Make your changes
4. Submit a pull request

Check out [good first issues](https://github.com/nikhil-robinson/openrouter_client/labels/good%20first%20issue) to get started.

## 📄 License & Support

**License**: [MIT](LICENSE) | **Issues**: [GitHub Issues](https://github.com/nikhil-robinson/openrouter_client/issues) | **Docs**: [OpenRouter API](https://openrouter.ai/docs)

---
*Built with ❤️ for the ESP32 community*
