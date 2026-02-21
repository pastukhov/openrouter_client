# OpenRouter ESP-IDF Examples

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.0%2B-blue)](https://idf.espressif.com/)
[![Examples](https://img.shields.io/badge/Examples-4-green)](.)

This directory contains comprehensive example applications demonstrating the full capabilities of the OpenRouter ESP-IDF component. Each example is designed to showcase specific features and can be used as a starting point for your own projects.

## 🚀 Quick Start

### Prerequisites

Before running any example, ensure you have:

1. **ESP-IDF 5.0+** installed and configured
2. **OpenRouter API Key** from [openrouter.ai](https://openrouter.ai)
3. **Wi-Fi Access Point** with internet connectivity
4. **ESP32 Development Board** (ESP32, ESP32-S3, ESP32-C3, etc.)

### Universal Setup Steps

All examples follow the same setup pattern:

1. **Navigate to Example Directory**:
   ```bash
   cd examples/[example_name]
   ```

2. **Configure Project**:
   ```bash
   idf.py menuconfig
   ```
   - Set **Wi-Fi SSID** and **Password** in "Example Configuration"
   - Set **OpenRouter API Key** in "Example Configuration"
   - Optionally adjust model parameters in "Component config → OpenRouter Client"

3. **Build and Flash**:
   ```bash
   idf.py build flash monitor
   ```

## 📋 Available Examples

### 🔤 Basic Text Examples

#### 1. **Non-Streaming Text Model** (`openrouter_text_model/`)
**Perfect for**: Simple Q&A, content generation, basic AI integration

**What it demonstrates:**
- ✅ Basic OpenRouter client setup and configuration
- ✅ Making synchronous API calls
- ✅ Processing complete responses at once
- ✅ Error handling and cleanup
- ✅ Wi-Fi connection management

**Use cases:**
- Chatbots with simple responses
- Content generation tools
- Question-answering systems
- One-shot AI queries

---

#### 2. **Streaming Text Model** (`openrouter_text_model_streaming/`)
**Perfect for**: Real-time chat interfaces, live content generation

**What it demonstrates:**
- ✅ Streaming response configuration
- ✅ Real-time token processing with callbacks
- ✅ Stream completion detection
- ✅ Memory-efficient token handling
- ✅ Responsive user interfaces

**Use cases:**
- Interactive chat applications
- Live text generation displays
- Real-time AI assistants
- Responsive web interfaces

---

### 🛠️ Advanced Examples

#### 3. **Function Calling** (`function_calling_example/`)
**Perfect for**: AI agents, smart home control, data integration

**What it demonstrates:**
- ✅ **Custom Function Registration**: Define your own functions with parameters
- ✅ **Automatic Tool Execution**: AI decides when and how to call functions
- ✅ **Parameter Validation**: Type checking and enum constraints
- ✅ **Multi-Function Workflows**: Chain multiple function calls
- ✅ **Error Handling**: Graceful function failure management

**Example Functions Included:**
- 🌤️ **Weather Service**: Mock weather data retrieval
- 🧮 **Math Calculator**: Mathematical expression evaluation
- 💡 **LED Controller**: Hardware control simulation

**Use cases:**
- Smart home automation
- IoT device control
- Data retrieval systems
- Multi-step AI workflows

**Code Preview:**
```c
// Define function parameters with validation
static const openrouter_param_t weather_params[] = {
    {"location", "string", "City and state, e.g. San Francisco, CA", true, NULL},
    {"unit", "string", "Temperature unit", false, (const char*[]){"celsius", "fahrenheit", NULL}},
    {NULL, NULL, NULL, false, NULL}
};

// Register function with simple API
openrouter_simple_function_t weather_func = {
    .name = "get_current_weather",
    .description = "Get the current weather in a given location",
    .parameters = weather_params,
    .callback = get_current_weather,
    .user_data = NULL
};
```

---

#### 4. **Multimodal Processing** (`multimodal_example/`)
**Perfect for**: Computer vision, audio processing, multimedia AI

**What it demonstrates:**
- ✅ **Image Analysis**: Process images from files, URLs, and memory
- ✅ **Audio Processing**: Handle audio files and transcription
- ✅ **Multi-format Support**: JPEG, PNG, MP3, WAV, and more
- ✅ **Combined Media**: Process images and audio together
- ✅ **SPIFFS Integration**: File system management
- ✅ **Both Modes**: Streaming and non-streaming multimodal calls

**Media Sources Supported:**
- 📁 **Local Files**: SPIFFS-stored images and audio
- 🌐 **URLs**: Remote media processing
- 💾 **Data Arrays**: In-memory media data
- 🔗 **Base64**: Encoded media content

**Use cases:**
- Security camera analysis
- Audio transcription systems
- Medical image analysis
- Content moderation
- Accessibility tools

**Code Preview:**
```c
// Add image from file
openrouter_add_image_file(handle, "/spiffs/camera_capture.jpg", 
                         "Describe what you see in this image");

// Add audio for transcription
openrouter_add_audio_file(handle, "/spiffs/voice_memo.mp3", 
                         "Transcribe this audio");

// Process both together
openrouter_call(handle, "Analyze the image and audio content", 
               response, sizeof(response));
```

## 🔧 Configuration Guide

### Example-Specific Configuration

Each example requires configuration through `idf.py menuconfig`:

#### **Wi-Fi Settings** (All Examples)
Navigate to: `Example Configuration`
- **Wi-Fi SSID**: Your network name
- **Wi-Fi Password**: Your network password

#### **API Configuration** (All Examples)  
Navigate to: `Example Configuration`
- **API Key**: Your OpenRouter API key from [openrouter.ai](https://openrouter.ai)

#### **Advanced Settings** (Optional)
Navigate to: `Component config → OpenRouter Client`
- **Response Buffer Size**: Adjust for larger responses (default: 4096)
- **Temperature**: Model creativity (0.0 = focused, 2.0 = creative)
- **Max Tokens**: Response length limit
- **HTTP Timeouts**: Network timeout settings

### Model Selection

Examples use different models optimized for their use case:

| Example | Default Model | Purpose |
|---------|---------------|---------|
| Text Model | `deepseek/deepseek-chat-v3-0324:free` | Fast, cost-effective text |
| Streaming | `deepseek/deepseek-chat-v3-0324:free` | Real-time responses |
| Function Calling | `openai/gpt-4o-mini-2024-07-18` | Reliable tool usage |
| Multimodal | `openai/gpt-4o` | Vision and audio support |

## 🏗️ Building and Running

### Standard Build Process

```bash
# Choose your example
cd examples/openrouter_text_model_streaming

# Configure (first time only)
idf.py menuconfig

# Build, flash, and monitor
idf.py build flash monitor
```

### Development Workflow

```bash
# Clean build (if needed)
idf.py fullclean

# Build only (faster iteration)
idf.py build

# Flash without building
idf.py flash

# Monitor serial output
idf.py monitor

# Exit monitor: Ctrl+]
```

### Memory Optimization

For memory-constrained applications:

```bash
# Enable compiler optimizations
idf.py menuconfig
# Compiler options → Optimization Level → Optimize for size (-Os)

# Reduce buffer sizes
# Component config → OpenRouter Client → Response Buffer Size → 2048
```

## 🧪 Testing Examples

### Expected Output Patterns

#### **Text Examples**:
```
I (2345) OPEN_ROUTER_EXAMPLE: connected to ap MyWiFi
I (3456) OPEN_ROUTER_EXAMPLE: Making API call...
I (7890) OPEN_ROUTER_EXAMPLE: Response: Why did the ESP32 go to therapy? Because it had too many chips on its shoulder!
```

#### **Function Calling**:
```
I (5678) OPEN_ROUTER_EXAMPLE: Registered 3 functions successfully
I (6789) OPEN_ROUTER_EXAMPLE: AI wants to call: get_current_weather
I (7890) OPEN_ROUTER_EXAMPLE: Function result: {"temperature": 22.5, "unit": "celsius"}
I (8901) OPEN_ROUTER_EXAMPLE: Final response: The current temperature is 22.5°C with partly cloudy conditions.
```

#### **Multimodal**:
```
I (4567) OPEN_ROUTER_EXAMPLE: Adding image from SPIFFS: /spiffs/test_image.png
I (5678) OPEN_ROUTER_EXAMPLE: Image analysis: This image shows a microcontroller board with various electronic components...
```

### Troubleshooting Common Issues

#### **Connection Problems**
```
E (1234) OPEN_ROUTER_EXAMPLE: connect to the AP fail
```
**Solutions:**
- Verify Wi-Fi SSID and password
- Check network signal strength
- Ensure 2.4GHz band availability

#### **API Errors**
```
E (2345) OPENROUTER: HTTP 401: Invalid API key
```
**Solutions:**
- Verify API key from OpenRouter dashboard
- Check for extra spaces in configuration
- Ensure sufficient API credits

#### **Memory Issues**
```
E (3456) OPENROUTER: Failed to allocate response buffer
```
**Solutions:**
- Reduce response buffer size in menuconfig
- Use streaming mode for large responses
- Increase partition sizes if needed

#### **Model Availability**
```
E (4567) OPENROUTER: HTTP 400: Model not found
```
**Solutions:**
- Check [OpenRouter Models](https://openrouter.ai/models) for availability
- Verify model name spelling
- Try alternative models in the same category

## 🔍 Understanding the Code

### Common Code Patterns

All examples follow these patterns:

#### **1. Wi-Fi Initialization**
```c
void wifi_init_sta(void) {
    // Create event group for connection status
    s_wifi_event_group = xEventGroupCreate();
    
    // Initialize networking stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Configure and start Wi-Fi
    // ... (standard ESP-IDF Wi-Fi setup)
}
```

#### **2. OpenRouter Configuration**
```c
openrouter_config_t config = {
    .api_key = CONFIG_API_KEY,              // From menuconfig
    .default_model = "chosen_model",         // Model selection
    .default_system_role = "system_prompt",  // AI behavior
    .temperature = 0.7f,                     // Creativity level
    .max_tokens = 1024,                      // Response length
    .enable_streaming = false,               // Mode selection
    .enable_tools = false                    // Function calling
};
```

#### **3. Error Handling**
```c
esp_err_t err = openrouter_call(handle, prompt, response, sizeof(response));
if (err == ESP_OK) {
    ESP_LOGI(TAG, "Success: %s", response);
} else {
    ESP_LOGE(TAG, "Failed: %s", esp_err_to_name(err));
}
```

### Customization Points

Each example can be easily modified:

#### **Change Models**
```c
// In config structure
.default_model = "anthropic/claude-3-sonnet",  // Anthropic model
.default_model = "google/gemini-pro",          // Google model
.default_model = "meta-llama/llama-3-8b",      // Meta model
```

#### **Adjust Parameters**
```c
.temperature = 0.0f,    // More focused responses
.temperature = 1.5f,    // More creative responses
.max_tokens = 2048,     // Longer responses
.top_p = 0.9f,         // Focused sampling
```

#### **Add Custom Functions** (Function Calling Example)
```c
// Define your function
char* my_custom_function(const char *name, const char *args, void *data) {
    // Your implementation here
    return strdup("{\"result\": \"success\"}");
}

// Register it
openrouter_simple_function_t my_func = {
    .name = "my_function",
    .description = "Does something useful",
    .parameters = my_params,
    .callback = my_custom_function
};
openrouter_register_simple_function(handle, &my_func);
```

## 📚 Learning Path

### Recommended Learning Order

1. **Start Here**: `openrouter_text_model/` - Learn basic concepts
2. **Add Streaming**: `openrouter_text_model_streaming/` - Real-time responses  
3. **Add Intelligence**: `function_calling_example/` - AI agents
4. **Add Media**: `multimodal_example/` - Vision and audio

### Next Steps

After mastering these examples:

- **Integrate with Hardware**: Add sensors, displays, motors
- **Build Applications**: Create complete IoT solutions
- **Optimize Performance**: Tune for your specific use case
- **Add Features**: Implement conversation memory, user preferences
- **Scale Up**: Handle multiple concurrent requests

## 🤝 Contributing Examples

We welcome new examples! To contribute:

1. **Fork the Repository**
2. **Create Example Directory**: `examples/your_example_name/`
3. **Follow Structure**: Use existing examples as templates
4. **Add Documentation**: Include detailed README.md
5. **Test Thoroughly**: Verify on multiple ESP32 variants
6. **Submit Pull Request**: Include description and screenshots

### Example Template Structure

```
examples/your_example/
├── CMakeLists.txt           # Project configuration
├── README.md               # Detailed documentation
├── main/
│   ├── CMakeLists.txt      # Main component config
│   ├── idf_component.yml   # Dependencies
│   ├── Kconfig.projbuild   # Configuration options
│   └── main.c              # Application code
└── sdkconfig.defaults      # Default settings
```

## 📞 Support

- 🐛 **Issues**: [GitHub Issues](https://github.com/pastukhov/openrouter_client/issues)
- 💬 **Discussions**: [GitHub Discussions](https://github.com/pastukhov/openrouter_client/discussions)
- 📖 **Documentation**: [Component README](../README.md)
- 🔗 **API Reference**: [OpenRouter Docs](https://openrouter.ai/docs)

---

<p align="center">
  <strong>Ready to build AI-powered ESP32 applications? Start with any example above! 🚀</strong>
</p>
- Implementing custom tool functions (weather, calculator, system info)
- Handling both formal OpenAI tool call format and custom formats
- Managing tool call state and execution
- Real-world function integration patterns

## Prerequisites

Before running these examples:

1. Make sure you have ESP-IDF installed and configured
2. Obtain an API key from [OpenRouter](https://openrouter.ai)
3. Configure your Wi-Fi credentials

## Configuration

Each example requires the following configuration:

1. Set your OpenRouter API key:
   ```
   idf.py menuconfig
   ```
   Navigate to "Example Configuration" and set:
   - API key
   - Wi-Fi SSID and password

2. (Optional) Adjust OpenRouter client parameters:
   ```
   idf.py menuconfig
   ```
   Navigate to "Component config → OpenRouter Client" to configure model parameters.

## Building and Running

### For Basic Text Model:

```bash
cd openrouter_text_model
idf.py build flash monitor
```

### For Streaming Text Model:

```bash
cd openrouter_text_model_streaming
idf.py build flash monitor
```

## Expected Output

### Basic Text Model

```
I (5271) OPEN_ROUTER_EXAMPLE: connected to ap SSID:MyWiFi password:********
I (7523) OPEN_ROUTER_EXAMPLE: Response: Why did the scarecrow win an award? Because he was outstanding in his field!
```

### Streaming Text Model

```
I (5334) OPEN_ROUTER_EXAMPLE: connected to ap SSID:MyWiFi password:********
Streaming response: Once upon a time, there was a small robot named Byte who lived in a laboratory...
[Content continues to stream token by token]
[STREAM COMPLETE]
```

## Troubleshooting

1. **Connection Issues**:
   - Verify your Wi-Fi credentials
   - Check that your OpenRouter API key is valid
   - Ensure your network allows HTTPS connections

2. **Memory Issues**:
   - If you see heap errors, try reducing the response buffer size in the example
   - For streaming examples, longer responses require more memory

3. **Model Selection**:
   - Some models may not be available with your OpenRouter account tier
   - Check the OpenRouter dashboard for available models

## Customizing the Examples

To modify these examples for your needs:

1. Change the model by updating the `default_model` field in the config structure
2. Adjust parameters like temperature and max_tokens for different response styles
3. For streaming examples, modify the callback function to process tokens differently

## Further Reading

For more details on the API and advanced usage, refer to:
- The main [README.md](../README.md) in the component root
- [OpenRouter API Documentation](https://openrouter.ai/docs)
