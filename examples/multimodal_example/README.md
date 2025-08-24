# OpenRouter Multimodal Example

This example demonstrates how to use the OpenRouter ESP-IDF library with multimodal content including images and audio.

## Features Demonstrated

- **Text Messages**: Simple text-only conversations
- **Image URLs**: Send images from URLs with different detail levels
- **Image Files**: Load and send images from local files (with automatic format detection and base64 encoding)
- **Audio Files**: Load and send audio files (with automatic format detection and base64 encoding)
- **Multimodal Conversations**: Complex conversations mixing text, images, and audio
- **Streaming**: Real-time streaming responses for multimodal content

## Supported Formats

### Images
- JPEG (.jpg, .jpeg)
- PNG (.png)
- GIF (.gif)
- WebP (.webp)

### Audio
- MP3 (.mp3)
- WAV (.wav)
- FLAC (.flac)
- OGG (.ogg)
- AAC (.aac)

## Configuration

1. Configure your WiFi credentials in `menuconfig`:
   ```
   Component config -> Example Configuration -> WiFi SSID
   Component config -> Example Configuration -> WiFi Password
   ```

2. Set your OpenRouter API key:
   ```
   Component config -> OpenRouter Configuration -> API Key
   ```

3. Choose a multimodal-capable model (default: `openai/gpt-4o`)

## File Structure

```
multimodal_example/
├── main/
│   ├── main.c              # Main application with examples
│   ├── CMakeLists.txt      # Component build configuration
│   └── idf_component.yml   # Component dependencies
├── CMakeLists.txt          # Project build configuration
└── README.md              # This file
```

## Usage Examples

### Simple Text Message

```c
openrouter_content_part_t text_part = openrouter_create_text_content("Hello, world!");

openrouter_multimodal_message_t message = {
    .role = "user",
    .parts = &text_part,
    .part_count = 1,
    .name = NULL
};

char response[1000];
esp_err_t err = openrouter_call_multimodal(handle, &message, 1, response, sizeof(response));
```

### Image from URL

```c
openrouter_content_part_t parts[2];
parts[0] = openrouter_create_text_content("What do you see in this image?");
parts[1] = openrouter_create_image_url_content("https://example.com/image.jpg", OPENROUTER_IMAGE_DETAIL_HIGH);

openrouter_multimodal_message_t message = {
    .role = "user",
    .parts = parts,
    .part_count = 2,
    .name = NULL
};

char response[1000];
esp_err_t err = openrouter_call_multimodal(handle, &message, 1, response, sizeof(response));
```

### Image from File

```c
FILE *image_file = fopen("/spiffs/sample_image.jpg", "rb");
if (image_file) {
    openrouter_content_part_t parts[2];
    parts[0] = openrouter_create_text_content("Analyze this image:");
    parts[1] = openrouter_create_image_file_content(image_file, OPENROUTER_IMAGE_DETAIL_AUTO);
    
    fclose(image_file);
    
    // Use the parts in a message...
    
    // Remember to clean up file-based content
    openrouter_free_content_part(&parts[1]);
}
```

### Audio from File

```c
FILE *audio_file = fopen("/spiffs/sample_audio.mp3", "rb");
if (audio_file) {
    openrouter_content_part_t parts[2];
    parts[0] = openrouter_create_text_content("Please transcribe this audio:");
    parts[1] = openrouter_create_audio_file_content(audio_file);
    
    fclose(audio_file);
    
    // Use the parts in a message...
    
    // Remember to clean up file-based content
    openrouter_free_content_part(&parts[1]);
}
```

### Streaming Response

```c
void streaming_callback(const char *content, bool is_complete, void *user_data) {
    if (content && strlen(content) > 0) {
        printf("%s", content);
    }
    if (is_complete) {
        printf("\n[Complete]\n");
    }
}

// Enable streaming
openrouter_set_streaming(handle, true);

// Make streaming call
esp_err_t err = openrouter_call_multimodal_streaming(handle, messages, message_count, streaming_callback, NULL);
```

## Memory Management

- **URL-based content**: No special cleanup required
- **File-based content**: Must call `openrouter_free_content_part()` to free allocated base64 data
- **Automatic format detection**: Formats are detected from file headers, not file extensions

## Model Compatibility

Not all models support multimodal content. Make sure to use models that support:
- **Vision**: For image analysis (e.g., `openai/gpt-4o`, `anthropic/claude-3-sonnet`)
- **Audio**: For audio processing (model support varies)

Check the OpenRouter documentation for the latest model capabilities.

## Error Handling

The library provides detailed error codes:
- File format detection failures
- Base64 encoding errors
- Memory allocation failures
- API-specific errors

Check return values and use `esp_err_to_name()` for debugging.

## Build and Flash

```bash
idf.py build
idf.py flash monitor
```
