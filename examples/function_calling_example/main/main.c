#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "openrouter.h"

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "OPEN_ROUTER_EXAMPLE";

// Example function: Get current weather
char* get_current_weather(const char *function_name, const char *arguments, void *user_data)
{
    ESP_LOGI(TAG, "Getting weather for location from arguments: %s", arguments);
    
    // In a real implementation, you would:
    // 1. Parse the JSON arguments to extract location and unit
    // 2. Make API call to weather service or read sensors
    // 3. Format response as JSON
    
    // For this example, return mock weather data
    return strdup("{\"temperature\": 22  .5, \"unit\": \"celsius\", \"description\": \"Partly cloudy\", \"humidity\": \"65%\"}");
}

// Example function: Calculate math operations
char* calculate_math(const char *function_name, const char *arguments, void *user_data)
{
    ESP_LOGI(TAG, "Calculating math expression from arguments: %s", arguments);
    
    // In a real implementation, you would:
    // 1. Parse the JSON to extract the expression
    // 2. Evaluate the mathematical expression safely
    // 3. Return result as JSON
    
    // For this example, return a mock calculation
    return strdup("{\"expression\": \"25 * 4\", \"result\": 100}");
}

// Example function: Control LED
char* control_led(const char *function_name, const char *arguments, void *user_data)
{
    ESP_LOGI(TAG, "Controlling LED with arguments: %s", arguments);
    
    // In a real implementation, you would:
    // 1. Parse arguments to get LED state (on/off) and color
    // 2. Control the actual LED hardware
    // 3. Return success/failure status
    
    // For this example, simulate LED control
    ESP_LOGI(TAG, "LED controlled successfully!");
    return strdup("{\"status\": \"success\", \"message\": \"LED state updated\"}");
}

// WiFi event handler

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "connect to the AP fail");
        esp_wifi_connect();
        ESP_LOGI(TAG, "retry to connect to the AP");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
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
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
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
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}


void openrouter_tooling_task(void *pvParameters)
{
    openrouter_config_t config = {
        .api_key = CONFIG_API_KEY,
        .default_model = "openai/gpt-4o-mini-2024-07-18",
        .default_system_role = "You are a helpful assistant that can get weather information, perform calculations, and control devices.",
        .temperature = 0.7,
        .max_tokens = 150,
        .enable_streaming = false,
        .enable_tools = true,  // Enable function calling
    };
    
    // Create OpenRouter handle
    openrouter_handle_t handle = openrouter_create(&config);
    if (!handle) {
        ESP_LOGE(TAG, "Failed to create OpenRouter handle");
        return;
    }
    
    // Define function parameters using the simplified API
    
    // Weather function parameters
    static const char* temp_units[] = {"celsius", "fahrenheit", NULL};
    static const openrouter_param_t weather_params[] = {
        {"location", "string", "The city and state, e.g. San Francisco, CA", true, NULL},
        {"unit", "string", "Temperature unit", false, temp_units},
        {NULL, NULL, NULL, false, NULL}  // Terminator
    };
    
    // Math function parameters  
    static const openrouter_param_t math_params[] = {
        {"expression", "string", "Mathematical expression to evaluate (e.g., '2 + 3', '10 * 5')", true, NULL},
        {NULL, NULL, NULL, false, NULL}  // Terminator
    };
    
    // LED control parameters
    static const char* led_states[] = {"on", "off", NULL};
    static const char* led_colors[] = {"red", "green", "blue", "white", NULL};
    static const openrouter_param_t led_params[] = {
        {"state", "string", "LED state", true, led_states},
        {"color", "string", "LED color", false, led_colors},
        {NULL, NULL, NULL, false, NULL}  // Terminator
    };
    
    // Register functions using the simplified API
    openrouter_simple_function_t weather_func = {
        .name = "get_current_weather",
        .description = "Get the current weather in a given location",
        .parameters = weather_params,
        .callback = get_current_weather,
        .user_data = NULL
    };
    
    openrouter_simple_function_t math_func = {
        .name = "calculate_math",
        .description = "Calculate the result of a mathematical expression",
        .parameters = math_params,
        .callback = calculate_math,
        .user_data = NULL
    };
    
    openrouter_simple_function_t led_func = {
        .name = "control_led",
        .description = "Control the LED state and color",
        .parameters = led_params,
        .callback = control_led,
        .user_data = NULL
    };
    
    // Register all functions
    esp_err_t err = openrouter_register_simple_function(handle, &weather_func);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register weather function: %s", esp_err_to_name(err));
    }
    
    err = openrouter_register_simple_function(handle, &math_func);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register math function: %s", esp_err_to_name(err));
    }
    
    err = openrouter_register_simple_function(handle, &led_func);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register LED function: %s", esp_err_to_name(err));
    }
    
    // Test function calling with various prompts
    char response[2048];
    
    ESP_LOGI(TAG, "=== Testing Function Calling ===");
    
    // Test 1: Weather query
    ESP_LOGI(TAG, "\n--- Test 1: Weather Query ---");
    const char *weather_prompt = "What's the weather like in New York?";
    ESP_LOGI(TAG, "Prompt: %s", weather_prompt);
    
    err = openrouter_call_with_tools(handle, weather_prompt, response, sizeof(response), 5);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Response: %s", response);
    } else {
        ESP_LOGE(TAG, "Weather query failed: %s", esp_err_to_name(err));
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Test 2: Math calculation
    ESP_LOGI(TAG, "\n--- Test 2: Math Calculation ---");
    const char *math_prompt = "What is 25 * 4?";
    ESP_LOGI(TAG, "Prompt: %s", math_prompt);
    
    err = openrouter_call_with_tools(handle, math_prompt, response, sizeof(response), 5);
    if (err == ESP_OK) { 
        ESP_LOGI(TAG, "Response: %s", response);
    } else {
        ESP_LOGE(TAG, "Math query failed: %s", esp_err_to_name(err));
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Test 3: LED control
    ESP_LOGI(TAG, "\n--- Test 3: LED Control ---");
    const char *led_prompt = "Turn on the LED with red color";
    ESP_LOGI(TAG, "Prompt: %s", led_prompt);
    
    err = openrouter_call_with_tools(handle, led_prompt, response, sizeof(response), 5);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Response: %s", response);
    } else {
        ESP_LOGE(TAG, "LED control failed: %s", esp_err_to_name(err));
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Test 4: Combined query
    ESP_LOGI(TAG, "\n--- Test 4: Combined Query ---");
    const char *combined_prompt = "What's the weather in London, calculate 15 + 27, and turn off the LED?";
    ESP_LOGI(TAG, "Prompt: %s", combined_prompt);
    
    err = openrouter_call_with_tools(handle, combined_prompt, response, sizeof(response), 5);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Response: %s", response);
    } else {
        ESP_LOGE(TAG, "Combined query failed: %s", esp_err_to_name(err));
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Test 5: Regular conversation (no function calls)
    ESP_LOGI(TAG, "\n--- Test 5: Regular Conversation ---");
    const char *regular_prompt = "Tell me a short joke about programming.";
    ESP_LOGI(TAG, "Prompt: %s", regular_prompt);
    
    err = openrouter_call_with_tools(handle, regular_prompt, response, sizeof(response), 5);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Response: %s", response);
    } else {
        ESP_LOGE(TAG, "Regular query failed: %s", esp_err_to_name(err));
    }
    
    // Cleanup
    openrouter_destroy(handle);
    
    ESP_LOGI(TAG, "Function calling examples completed!");
    vTaskDelete(NULL);
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    xTaskCreate(openrouter_tooling_task, "openrouter_tooling_task", 8192, NULL, 5, NULL);
}