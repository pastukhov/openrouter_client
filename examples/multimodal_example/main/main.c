#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "openrouter.h"

#include "esp_spiffs.h"

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "OPEN_ROUTER_EXAMPLE";

/* Example file paths - adjust these to your actual files */
#define EXAMPLE_IMAGE_PATH "/spiffs/test_image.png"
#define EXAMPLE_AUDIO_PATH "/spiffs/test_audio.mp3"

/* Example URLs for demonstration */
#define EXAMPLE_IMAGE_URL "https://commons.wikimedia.org/wiki/File:Seed_Head_(213442685).jpeg"
#define EXAMPLE_AUDIO_URL "https://example.com/audio.mp3"

/* Response buffer for non-streaming calls */
#define RESPONSE_BUFFER_SIZE 4096
static char response_buffer[RESPONSE_BUFFER_SIZE];

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "connect to the AP %s fail", CONFIG_WIFI_SSID);
        esp_wifi_connect();
        ESP_LOGI(TAG, "retry to connect to the AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = CONFIG_WIFI_SSID,
                .password = CONFIG_WIFI_PASSWORD,
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits =
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}


/**
 * @brief Streaming callback function
 */
static void streaming_callback(const char *content, bool is_complete, void *user_data)
{
    if (content && strlen(content) > 0) {
        printf("%s", content);
        fflush(stdout);
    }
    
    if (is_complete) {
        printf("\n[STREAM COMPLETE]\n");
    }
}


/**
 * @brief Example 1: Analyze an image from local file (non-streaming)
 */
static void example_image_analysis(openrouter_handle_t handle)
{
    ESP_LOGI(TAG, "=== Example 1: Image Analysis (Local File) ===");
    
    const char *prompt = "What do you see in this image? Please describe it in detail.";
    
    esp_err_t err = openrouter_call_with_image(handle, prompt, EXAMPLE_IMAGE_PATH, 
                                              response_buffer, sizeof(response_buffer));
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Image analysis response: %s", response_buffer);
    } else {
        ESP_LOGE(TAG, "Image analysis failed: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Example 2: Analyze an image from URL (streaming)
 */
static void example_image_url_streaming(openrouter_handle_t handle)
{
    ESP_LOGI(TAG, "=== Example 2: Image Analysis (URL, Streaming) ===");
    
    const char *prompt = "Analyze this image and tell me what you see.";
    
    esp_err_t err = openrouter_call_with_image_url_streaming(handle, prompt, EXAMPLE_IMAGE_URL,
                                                        streaming_callback, NULL);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Streaming image analysis failed: %s", esp_err_to_name(err));
    }
}
/**
 * @brief Example 3: Process audio file (non-streaming)
 */
static void example_audio_processing(openrouter_handle_t handle)
{
    ESP_LOGI(TAG, "=== Example 3: Audio Processing (Local File) ===");
    
    const char *prompt = "Please transcribe and summarize the content of this audio file.";
    
    esp_err_t err = openrouter_call_with_audio(handle, prompt, EXAMPLE_AUDIO_PATH,
                                              response_buffer, sizeof(response_buffer));
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Audio processing response: %s", response_buffer);
    } else {
        ESP_LOGE(TAG, "Audio processing failed: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Example 4: Process audio from URL (streaming)
 */
static void example_audio_url_streaming(openrouter_handle_t handle)
{
    ESP_LOGI(TAG, "=== Example 4: Audio Processing (URL, Streaming) ===");
    
    const char *prompt = "Listen to this audio and provide a summary.";
    
    esp_err_t err = openrouter_call_with_audio_url_streaming(handle, prompt, EXAMPLE_AUDIO_URL,
                                                        streaming_callback, NULL);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Streaming audio processing failed: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Example 5: Combined image and audio processing (non-streaming)
 */
static void example_combined_media(openrouter_handle_t handle)
{
    ESP_LOGI(TAG, "=== Example 5: Combined Image and Audio Processing ===");
    
    const char *prompt = "Please analyze both the image and audio content. "
                        "Describe what you see in the image and what you hear in the audio, "
                        "then tell me if there are any connections between them.";
    
    esp_err_t err = openrouter_call_with_media(handle, prompt, EXAMPLE_IMAGE_PATH, EXAMPLE_AUDIO_PATH,
                                              response_buffer, sizeof(response_buffer));
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Combined media response: %s", response_buffer);
    } else {
        ESP_LOGE(TAG, "Combined media processing failed: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Example 6: Regular text-only call (to show compatibility)
 */
static void example_text_only(openrouter_handle_t handle)
{
    ESP_LOGI(TAG, "=== Example 6: Regular Text-Only Call (Compatibility) ===");
    
    const char *prompt = "Hello! Please explain what multimodal AI is in simple terms.";
    
    esp_err_t err = openrouter_call(handle, prompt, response_buffer, sizeof(response_buffer));
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Text-only response: %s", response_buffer);
    } else {
        ESP_LOGE(TAG, "Text-only call failed: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Example 7: Text-only with streaming (to show compatibility)
 */
static void example_text_streaming(openrouter_handle_t handle)
{
    ESP_LOGI(TAG, "=== Example 7: Text-Only Streaming (Compatibility) ===");
    
    const char *prompt = "Write a short story about an AI that can see and hear.";
    
    esp_err_t err = openrouter_call_streaming(handle, prompt, streaming_callback, NULL);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Text-only streaming failed: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Demonstrate error handling for various scenarios
 */
static void example_error_handling(openrouter_handle_t handle)
{
    ESP_LOGI(TAG, "=== Example 8: Error Handling Demonstration ===");
    
    /* Test with non-existent file */
    esp_err_t err = openrouter_call_with_image(handle, "What's in this image?", "/nonexistent/file.jpg",
                                              response_buffer, sizeof(response_buffer));
    ESP_LOGI(TAG, "Non-existent file error: %s", esp_err_to_name(err));
    
    /* Test with unsupported format */
    err = openrouter_call_with_image(handle, "What's in this image?", "/spiffs/document.pdf",
                                    response_buffer, sizeof(response_buffer));
    ESP_LOGI(TAG, "Unsupported format error: %s", esp_err_to_name(err));
    
    /* Test with no media provided */
    err = openrouter_call_with_media(handle, "Process this media", NULL, NULL,
                                    response_buffer, sizeof(response_buffer));
    ESP_LOGI(TAG, "No media provided error: %s", esp_err_to_name(err));
}

/**
 * @brief Example 9: Using image and audio data arrays (simulated)
 */
static void example_data_arrays(openrouter_handle_t handle)
{
    ESP_LOGI(TAG, "=== Example 9: Data Arrays (Simulated) ===");
    
    // Simulate JPEG header for demonstration
    unsigned char jpeg_header[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46};
    
    esp_err_t err = openrouter_call_with_image_data(handle, 
                                                   "This is a simulated JPEG header. What can you tell me about JPEG format?",
                                                   jpeg_header, sizeof(jpeg_header),
                                                   response_buffer, sizeof(response_buffer));
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Data array response: %s", response_buffer);
    } else {
        ESP_LOGE(TAG, "Data array processing failed: %s", esp_err_to_name(err));
    }
    
    // Simulate WAV header for audio demonstration
    unsigned char wav_header[] = {'R', 'I', 'F', 'F', 0x24, 0x08, 0x00, 0x00, 'W', 'A', 'V', 'E'};
    
    err = openrouter_call_with_audio_data(handle,
                                         "This is a simulated WAV header. What can you tell me about WAV format?",
                                         wav_header, sizeof(wav_header),
                                         response_buffer, sizeof(response_buffer));
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Audio data array response: %s", response_buffer);
    } else {
        ESP_LOGE(TAG, "Audio data array processing failed: %s", esp_err_to_name(err));
    }
}


void openrouter_multimodal_task(void *pvParameters)
{
    /* Configure OpenRouter for non-streaming mode */
    openrouter_config_t config = {
        .api_key = CONFIG_API_KEY,
        .default_model = "mistralai/mistral-small-3.2-24b-instruct:free",  // Use a multimodal-capable model
        .enable_streaming = false,  // Start with non-streaming
        .temperature = 0.7f,
        .max_tokens = 1000,
        .response_buffer_size = 8192,
        .http_timeout_ms = 60000,  // Longer timeout for multimodal requests
        .enable_connection_reuse = true
    };
    
    /* Create OpenRouter handle */
    openrouter_handle_t handle = openrouter_create(&config);
    if (!handle) {
        ESP_LOGE(TAG, "Failed to create OpenRouter handle");
        return;
    }
    
    ESP_LOGI(TAG, "OpenRouter client created successfully");
    
    /* Run non-streaming examples */
    example_image_analysis(handle);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    example_audio_processing(handle);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    example_combined_media(handle);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    example_text_only(handle);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    /* Switch to streaming mode */
    ESP_LOGI(TAG, "Switching to streaming mode...");
    openrouter_set_streaming(handle, true);
    
    /* Run streaming examples */
    example_image_url_streaming(handle);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    example_audio_url_streaming(handle);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    example_text_streaming(handle);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    /* Switch back to non-streaming for error examples */
    openrouter_set_streaming(handle, false);
    example_error_handling(handle);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    example_data_arrays(handle);
    
    /* Cleanup */
    openrouter_destroy(handle);

    ESP_LOGI(TAG, "OpenRouter client destroyed");

    vTaskDelete(NULL);
}


void init_spiffs()
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs", .partition_label = NULL, .max_files = 5, .format_if_mount_failed = false};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_spiffs();

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    xTaskCreate(openrouter_multimodal_task, "openrouter_multimodal_task", 16384, NULL, 5, NULL);
}
