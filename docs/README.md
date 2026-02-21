# OpenRouter ESP-IDF API Documentation

Welcome to the comprehensive API documentation for the OpenRouter ESP-IDF client library. This documentation provides detailed information about all available functions, data structures, and usage patterns.

## 📚 Documentation Structure

### Core Documentation
- **[API Reference](api-reference.md)** - Complete function reference with parameters and return values
- **[Data Structures](data-structures.md)** - All structs, enums, and type definitions
- **[Configuration Guide](configuration.md)** - Detailed configuration options and best practices
- **[Error Handling](error-handling.md)** - Error codes, troubleshooting, and debugging

### Feature Guides
- **[Basic Usage](basic-usage.md)** - Getting started with simple text generation
- **[Streaming Guide](streaming.md)** - Real-time streaming responses implementation
- **[Function Calling](function-calling.md)** - AI function calling and tool integration
- **[Multimodal Support](multimodal.md)** - Image and audio processing capabilities

### Advanced Topics
- **[Memory Management](memory-management.md)** - Buffer handling and optimization
- **[Threading & Safety](threading.md)** - Thread safety and concurrent usage
- **[Performance Optimization](performance.md)** - Tips for optimal performance
- **[Security Considerations](security.md)** - Best practices for secure usage

### Development
- **[Migration Guide](migration.md)** - Upgrading from previous versions
- **[Contributing](contributing.md)** - Guidelines for contributing to the library
- **[Changelog](../CHANGELOG.md)** - Version history and changes

## 🚀 Quick Navigation

### I want to...
- **[Make a simple API call](basic-usage.md#simple-api-call)** → Basic Usage
- **[Stream responses in real-time](streaming.md#basic-streaming)** → Streaming Guide  
- **[Let AI call my functions](function-calling.md#registering-functions)** → Function Calling
- **[Process images with AI](multimodal.md#image-processing)** → Multimodal Support
- **[Handle errors properly](error-handling.md#error-handling-patterns)** → Error Handling
- **[Optimize memory usage](memory-management.md#buffer-optimization)** → Memory Management
- **[Configure the library](configuration.md#menuconfig-options)** → Configuration Guide

### By Use Case
- **Chatbots & Assistants** → [Basic Usage](basic-usage.md) + [Streaming](streaming.md)
- **Smart IoT Devices** → [Function Calling](function-calling.md) + [Configuration](configuration.md)
- **Computer Vision** → [Multimodal Support](multimodal.md) + [Performance](performance.md)
- **Real-time Applications** → [Streaming](streaming.md) + [Threading](threading.md)

## 📖 Getting Started

If you're new to the OpenRouter ESP-IDF library, we recommend following this learning path:

1. **[Installation & Setup](../README.md#quick-start)** - Set up the library in your project
2. **[Basic Usage](basic-usage.md)** - Make your first API call
3. **[Configuration](configuration.md)** - Configure for your needs
4. **[Examples](../examples/README.md)** - Try the provided examples
5. **Advanced Features** - Explore streaming, function calling, and multimodal support

## 🔍 API Overview

The OpenRouter ESP-IDF library provides a comprehensive C API for interacting with OpenRouter's AI models:

### Core Functions
```c
// Handle management
openrouter_handle_t openrouter_create(const openrouter_config_t *config);
void openrouter_destroy(openrouter_handle_t handle);

// Basic API calls
esp_err_t openrouter_call(openrouter_handle_t handle, const char *prompt, 
                         char *response, size_t response_size);

// Streaming API calls
esp_err_t openrouter_call_streaming(openrouter_handle_t handle, const char *prompt,
                                   openrouter_stream_callback_t callback, void *user_data);
```

### Advanced Features
```c
// Function calling
esp_err_t openrouter_register_function(openrouter_handle_t handle, 
                                      const openrouter_function_t *function);
esp_err_t openrouter_call_with_tools(openrouter_handle_t handle, const char *prompt,
                                    char *response, size_t response_size, int max_iterations);

// Multimodal support
esp_err_t openrouter_call_with_image(openrouter_handle_t handle, const char *prompt,
                                    const char *image_path, char *response, size_t response_size);
```

## 📋 Supported Features

| Feature | Description | Documentation |
|---------|-------------|---------------|
| ✅ **Text Generation** | Basic AI text completion | [Basic Usage](basic-usage.md) |
| ✅ **Streaming Responses** | Real-time token streaming | [Streaming Guide](streaming.md) |
| ✅ **Function Calling** | AI can call your functions | [Function Calling](function-calling.md) |
| ✅ **Image Processing** | Vision AI capabilities | [Multimodal Support](multimodal.md) |
| ✅ **Audio Processing** | Audio transcription and analysis | [Multimodal Support](multimodal.md) |
| ✅ **Connection Pooling** | HTTP connection reuse | [Performance](performance.md) |
| ✅ **Error Recovery** | Automatic retry with backoff | [Error Handling](error-handling.md) |
| ✅ **Thread Safety** | Concurrent usage support | [Threading](threading.md) |
| ✅ **Memory Optimization** | Dynamic buffer management | [Memory Management](memory-management.md) |

## 🔗 External Resources

- **[OpenRouter API Documentation](https://openrouter.ai/docs)** - Official OpenRouter API docs
- **[ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)** - ESP-IDF documentation
- **[GitHub Repository](https://github.com/pastukhov/openrouter-esp-idf)** - Source code and issues
- **[Examples](../examples/)** - Practical implementation examples

## 📞 Support

- 🐛 **Bug Reports**: [GitHub Issues](https://github.com/pastukhov/openrouter-esp-idf/issues)
- 💬 **Questions**: [GitHub Discussions](https://github.com/pastukhov/openrouter-esp-idf/discussions)
- 📖 **Documentation Issues**: Please report in GitHub Issues with the "documentation" label

---

<p align="center">
  <strong>Ready to build AI-powered ESP32 applications? Start with the <a href="basic-usage.md">Basic Usage Guide</a>! 🚀</strong>
</p>
