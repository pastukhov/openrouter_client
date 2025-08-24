# OpenRouter Non-Streaming Example

This example demonstrates how to use the OpenRouter ESP-IDF component to make non-streaming API calls to AI models through OpenRouter's platform.

## Overview

This application:
1. Connects to Wi-Fi
2. Initializes the OpenRouter client with a non-streaming configuration
3. Makes a simple API call with a prompt
4. Displays the complete response

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
I (9885) OPENROUTER: OpenRouter client created with model: deepseek/deepseek-chat-v3-0324:free, streaming: disabled
I (14685) OPEN_ROUTER_EXAMPLE: Response: Why did the scarecrow win an award? Because he was outstanding in his field!
```

## How It Works

1. The application first initializes NVS flash and Wi-Fi using the ESP-IDF Wi-Fi station example code.

2. Once connected to Wi-Fi, it creates an OpenRouter client configuration structure:
   ```c
   openrouter_config_t config = {
       .api_key = CONFIG_API_KEY,
       .default_model = "deepseek/deepseek-chat-v3-0324:free", // Model selection
       .default_system_role = "You are a helpful assistant.",   // System role
       .temperature = 0.7f,                                    // Temperature
       .max_tokens = 1024,                                     // Max response tokens
       .top_p = 1.0f,                                          // Top-p sampling
       .seed = -1,                                             // Random seed
       .response_buffer_size = 500,                            // Response buffer size
       .enable_streaming = false                               // Non-streaming mode
   };
   ```

3. It initializes the OpenRouter client with this configuration.

4. The application makes a simple API call asking for a joke:
   ```c
   esp_err_t err = openrouter_call(handle, "Tell me a joke", response, sizeof(response));
   ```

5. The response is logged to the console when received.

6. Finally, the OpenRouter client is properly destroyed to free resources.

## Troubleshooting

* If you see "Failed to create OpenRouter handle", check that your API key is correct.
* If you see "API call failed", check your internet connectivity and API key permissions.
* If responses are truncated, increase the response buffer size in the configuration.

## Further Reading

* See the [OpenRouter ESP-IDF README](../../README.md) for complete API documentation
* Visit [OpenRouter](https://openrouter.ai) for information on available models and pricing
