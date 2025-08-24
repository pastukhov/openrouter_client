# OpenRouter Streaming Example

This example demonstrates how to use the OpenRouter ESP-IDF component to make streaming API calls to AI models through OpenRouter's platform, receiving response tokens in real-time.

## Overview

This application:
1. Connects to Wi-Fi
2. Initializes the OpenRouter client with a streaming configuration
3. Makes a streaming API call with a prompt
4. Processes and displays tokens as they arrive in real-time
5. Indicates when the stream is complete

## Hardware Required

* An ESP32 development board
* A USB cable for power and programming
* Wi-Fi access point with internet connection

## Configuration

1. Open the project configuration menu:
   ```
   idf.py menuconfig
   ```

2. Configure Wi-Fi under "Example Configuration":
   * Set your Wi-Fi SSID
   * Set your Wi-Fi password

3. Configure OpenRouter:
   * Set your OpenRouter API key (obtain from [OpenRouter](https://openrouter.ai))
   * Optionally adjust model parameters under "Component config → OpenRouter Client"

## Build and Flash

1. Build the project:
   ```
   idf.py build
   ```

2. Flash the application to your device:
   ```
   idf.py -p [PORT] flash monitor
   ```
   Replace `[PORT]` with your device's serial port (e.g., COM3 on Windows, /dev/ttyUSB0 on Linux)

## Example Output

```
I (305) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (315) OPEN_ROUTER_EXAMPLE: ESP_WIFI_MODE_STA
I (325) wifi station: wifi_init_sta finished.
I (335) wifi:wifi driver task: 3ffc1db0, prio:23, stack:6656, core=0
I (335) system_api: Base MAC address is not set
I (335) system_api: read default base MAC address from EFUSE
I (355) wifi:wifi firmware version: 5f14a1a
I (355) wifi:wifi certification version: v7.0
I (355) wifi:config NVS flash: enabled
I (355) wifi:config nano formating: disabled
I (365) wifi:Init data frame dynamic rx buffer num: 32
I (365) wifi:Init management frame dynamic rx buffer num: 32
I (375) wifi:Init management short buffer num: 32
I (375) wifi:Init dynamic tx buffer num: 32
I (385) wifi:Init static rx buffer size: 1600
I (385) wifi:Init static rx buffer num: 10
I (385) wifi:Init dynamic rx buffer num: 32
I (395) wifi_init: rx ba win: 6
I (395) wifi_init: tcpip mbox: 32
I (405) wifi_init: udp mbox: 6
I (405) wifi_init: tcp mbox: 6
I (405) wifi_init: tcp tx win: 5744
I (415) wifi_init: tcp rx win: 5744
I (415) wifi_init: tcp mss: 1440
I (425) wifi_init: WiFi IRAM OP enabled
I (425) wifi_init: WiFi RX IRAM OP enabled
I (435) example_connect: Connecting to MyWiFi...
I (435) phy_init: phy_version 4670,719f9f6,Feb 18 2021,17:07:07
I (545) wifi:mode : sta (30:ae:a4:80:11:d0)
I (545) wifi:enable tsf
I (545) example_connect: Waiting for IP(s)
I (3995) wifi:new:<8,0>, old:<1,0>, ap:<255,255>, sta:<8,0>, prof:1
I (5275) wifi:state: init -> auth (b0)
I (5285) wifi:state: auth -> assoc (0)
I (5295) wifi:state: assoc -> run (10)
I (5335) wifi:connected with MyWiFi, aid = 2, channel 8, BW20, bssid = a4:cf:12:67:b4:a7
I (5335) wifi:security: WPA2-PSK, phy: bgn, rssi: -48
I (5345) wifi:pm start, type: 1

I (5355) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (5355) example_connect: Got IPv6 event: Interface "example_connect: sta" address: fe80:0000:0000:0000:32ae:a4ff:fe80:11d0, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (7855) esp_netif_handlers: example_connect: sta ip: 192.168.0.103, mask: 255.255.255.0, gw: 192.168.0.1
I (7855) example_connect: Got IPv4 event: Interface "example_connect: sta" address: 192.168.0.103
I (7865) example_connect: Connected to example_connect: sta
I (7865) example_connect: - IPv4 address: 192.168.0.103
I (7875) example_connect: - IPv6 address: fe80:0000:0000:0000:32ae:a4ff:fe80:11d0, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (7885) OPEN_ROUTER_EXAMPLE: connected to ap SSID:MyWiFi password:********
I (9885) OPENROUTER: OpenRouter client created with model: deepseek/deepseek-chat-v3-0324:free, streaming: enabled
Streaming response: Once upon a time, there was a small robot named Byte who lived in a laboratory. Byte was designed to help scientists with their experiments, but he always dreamed of exploring the world outside.

One day, when all the scientists had gone home, Byte noticed that someone had left the laboratory door slightly open. Curious, he wheeled himself toward the door and peeked outside. The night sky was filled with stars, and Byte's sensors had never detected anything so beautiful.

Gathering his courage, Byte rolled outside into the garden. He analyzed the flowers, recorded the sounds of crickets, and calculated the patterns of the stars above. Everything was new and fascinating.

As dawn approached, Byte realized he should return to the laboratory before the scientists arrived. On his way back, he encountered a lost kitten. Using his built-in GPS, Byte helped guide the kitten back to its home at the security guard's booth.

When Byte finally returned to the lab, the scientists were surprised to find soil on his wheels and a flower petal stuck to his sensor. But they were even more surprised when they checked his memory banks and discovered all the wonderful data he had collected.

From that day on, the scientists allowed Byte to explore the garden every evening. They realized that a robot with curiosity could make even better discoveries than they had programmed him for. And Byte learned that sometimes, the best algorithms are the ones that develop when you venture beyond your comfort zone.

[STREAM COMPLETE]
```

## How It Works

1. The application first initializes NVS flash and Wi-Fi using the ESP-IDF Wi-Fi station example code.

2. Once connected to Wi-Fi, it creates an OpenRouter client configuration structure with streaming enabled:
   ```c
   openrouter_config_t config = {
       .api_key = CONFIG_API_KEY,
       .default_model = "deepseek/deepseek-chat-v3-0324:free", // Model selection
       .default_system_role = "You are a helpful assistant.",   // System role
       .temperature = 0.7f,                                    // Temperature
       .max_tokens = 1024,                                     // Max response tokens
       .top_p = 1.0f,                                          // Top-p sampling
       .seed = -1,                                             // Random seed
       .response_buffer_size = 4096,                           // Response buffer size
       .enable_streaming = true                                // Enable streaming mode
   };
   ```

3. It defines a callback function to handle streaming tokens:
   ```c
   void stream_callback(const char* content, bool is_complete, void* user_data) {
       if (is_complete) {
           ESP_LOGI(TAG, "\n[STREAM COMPLETE]");
       } else {
           printf("%s", content); // Print delta content as it arrives
           fflush(stdout);
       }
   }
   ```

4. The application makes a streaming API call:
   ```c
   esp_err_t err = openrouter_call_streaming(handle, "Tell me a short story about a robot.",
                                          stream_callback, NULL);
   ```

5. As tokens arrive, they are processed by the callback function and printed immediately.

6. When the stream is complete, the callback is called with `is_complete=true`.

7. Finally, the OpenRouter client is properly destroyed to free resources.

## Benefits of Streaming

* Immediate response: Users see content as soon as it's generated
* Better user experience: No waiting for the complete response
* Memory efficiency: Process tokens incrementally rather than storing the entire response
* Early termination: You can potentially end the generation early if needed

## Troubleshooting

* If you see "Failed to create OpenRouter handle", check that your API key is correct.
* If you see "Streaming API call failed", check your internet connectivity and API key permissions.
* If the streaming response stops unexpectedly, check your network stability.

## Further Reading

* See the [OpenRouter ESP-IDF README](../../README.md) for complete API documentation
* Visit [OpenRouter](https://openrouter.ai) for information on available models and pricing
