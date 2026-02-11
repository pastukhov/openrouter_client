#ifndef OPENROUTER_H
#define OPENROUTER_H

#include "cJSON.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h> /* For size_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default values for OpenRouter configuration
 */
#define OPENROUTER_DEFAULT_RESPONSE_BUFFER_SIZE CONFIG_OPENROUTER_DEFAULT_RESPONSE_BUFFER_SIZE
#define OPENROUTER_DEFAULT_TEMPERATURE atof(CONFIG_OPENROUTER_DEFAULT_TEMPERATURE)
#define OPENROUTER_DEFAULT_MAX_TOKENS CONFIG_OPENROUTER_DEFAULT_MAX_TOKENS
#define OPENROUTER_DEFAULT_TOP_P atof(CONFIG_OPENROUTER_DEFAULT_TOP_P)
#define OPENROUTER_DEFAULT_HTTP_TIMEOUT CONFIG_OPENROUTER_DEFAULT_HTTP_TIMEOUT
#define OPENROUTER_DEFAULT_HTTP_TIMEOUT_STREAMING CONFIG_OPENROUTER_DEFAULT_HTTP_TIMEOUT_STREAMING

#ifdef CONFIG_OPENROUTER_DEFAULT_CONNECTION_REUSE
#define OPENROUTER_DEFAULT_CONNECTION_REUSE CONFIG_OPENROUTER_DEFAULT_CONNECTION_REUSE
#else
#define OPENROUTER_DEFAULT_CONNECTION_REUSE false
#endif

#ifdef CONFIG_OPENROUTER_DEFAULT_CONNECTION_TIMEOUT
#define OPENROUTER_DEFAULT_CONNECTION_TIMEOUT CONFIG_OPENROUTER_DEFAULT_CONNECTION_TIMEOUT
#else
#define OPENROUTER_DEFAULT_CONNECTION_TIMEOUT 30000
#endif

/**
 * @brief OpenRouter handle opaque pointer
 */
typedef struct openrouter_handle *openrouter_handle_t;

/**
 * @brief Streaming callback function type
 *
 * This callback is invoked for each chunk of data received during streaming.
 * The callback may be called multiple times as data arrives.
 *
 * @param content Delta text from the streaming response. This is just the new
 *                content since the last callback. Empty string indicates end of stream.
 *                May contain an error message in case of failure (prefixed with [ERROR]).
 * @param is_complete true when stream ends (final callback), false for intermediate chunks
 * @param user_data User-provided context data that was passed to the streaming call function
 */
typedef void (*openrouter_stream_callback_t)(const char *content, bool is_complete, void *user_data);

/**
 * @brief Function call callback type
 *
 * This callback is invoked when the AI wants to call a registered function.
 * The implementation should execute the function with the provided arguments
 * and return a JSON string with the result.
 *
 * @param function_name Name of the function being called
 * @param arguments JSON string containing the function arguments
 * @param user_data User-provided context data that was passed during function registration
 * @return char* JSON string containing the function result (must be freed by caller), or NULL on error
 */
typedef char *(*openrouter_function_callback_t)(const char *function_name, const char *arguments, void *user_data);

/**
 * @brief Simple function parameter definition
 */
typedef struct {
    const char *name;         /**< Parameter name */
    const char *type;         /**< Parameter type: "string", "number", "boolean", "array", "object" */
    const char *description;  /**< Parameter description */
    bool required;            /**< Whether this parameter is required */
    const char **enum_values; /**< Array of allowed string values (NULL-terminated), NULL if not enum */
} openrouter_param_t;

/**
 * @brief Simplified function definition for easy registration
 */
typedef struct {
    const char *name;                        /**< Function name */
    const char *description;                 /**< Function description */
    const openrouter_param_t *parameters;    /**< Array of parameters (NULL-terminated) */
    openrouter_function_callback_t callback; /**< Function implementation callback */
    void *user_data;                         /**< User data passed to callback */
} openrouter_simple_function_t;

/**
 * @brief Tool call callback type
 *
 * This callback is invoked when a tool call is received during streaming or non-streaming calls.
 * It provides information about tool calls that need to be executed.
 *
 * @param tool_call_id Unique identifier for this tool call
 * @param function_name Name of the function being called
 * @param arguments JSON string containing the function arguments
 * @param user_data User-provided context data
 * @return char* JSON string containing the function result (must be freed by caller), or NULL on error
 */
typedef char *(*openrouter_tool_callback_t)(const char *tool_call_id, const char *function_name, const char *arguments,
                                            void *user_data);

/**
 * @brief Function definition structure
 *
 * This structure defines a function that can be called by the AI.
 * It includes the function schema and metadata needed for the API.
 */
typedef struct {
    const char *name;                        /**< Function name */
    const char *description;                 /**< Function description */
    cJSON *parameters;                       /**< JSON schema for function parameters */
    openrouter_function_callback_t callback; /**< Function implementation callback */
    void *user_data;                         /**< User data passed to callback */
} openrouter_function_t;

/**
 * @brief Configuration structure for OpenRouter client
 *
 * This structure contains all the configuration parameters for initializing
 * an OpenRouter client. Only api_key and enable_streaming are required; all other
 * fields can be left as 0/NULL to use default values.
 */
typedef struct {
    const char *api_key;             /**< Required: OpenRouter API key for authentication */
    const char *default_model;       /**< Optional: Default model to use (NULL for "openai/gpt-4o-mini") */
    const char *default_system_role; /**< Optional: Default system message (NULL for none) */
    float temperature; /**< Optional: Sampling temperature (0.0-2.0). Higher values produce more random outputs */
    int max_tokens;    /**< Optional: Maximum tokens to generate in the response */
    float top_p;       /**< Optional: Nucleus sampling parameter (0.0-1.0). Affects token selection diversity */
    int seed;          /**< Optional: Random seed (-1 for random, other values for deterministic outputs) */
    size_t response_buffer_size;              /**< Optional: Response buffer size in bytes */
    bool enable_streaming;                    /**< Required: Enable/disable streaming responses */
    uint32_t http_timeout_ms;                 /**< Optional: HTTP timeout in milliseconds (0 for default) */
    const char *http_referer;                 /**< Optional: HTTP-Referer header value (NULL for none) */
    const char *x_title;                      /**< Optional: X-Title header value (NULL for none) */
    bool enable_tools;                        /**< Optional: Enable/disable function calling/tools */
    openrouter_tool_callback_t tool_callback; /**< Optional: Callback for handling tool calls */
    void *tool_user_data;                     /**< Optional: User data for tool callback */
    bool enable_connection_reuse;             /**< Optional: Enable/disable HTTP connection reuse (default: false) */
    uint32_t connection_timeout_ms;           /**< Optional: Connection keep-alive timeout in ms (default: 30000) */
} openrouter_config_t;

/**
 * @brief Create a new OpenRouter client handle
 *
 * This function creates and initializes a new OpenRouter client handle.
 * The handle must be destroyed with openrouter_destroy() when no longer needed.
 *
 * @note This library is not thread-safe. A single handle should not be used
 * concurrently from multiple tasks. Each task should create its own handle
 * or use appropriate synchronization mechanisms.
 *
 * @param config Pointer to configuration structure
 * @return openrouter_handle_t Handle on success, NULL on failure
 */
openrouter_handle_t openrouter_create(const openrouter_config_t *config);

/**
 * @brief Destroy an OpenRouter client handle and free all resources
 *
 * @param handle OpenRouter handle to destroy
 */
void openrouter_destroy(openrouter_handle_t handle);

/* Configuration setters */

/**
 * @brief Set the model to use for API calls
 *
 * @param handle OpenRouter handle
 * @param model Model name (e.g., "openai/gpt-3.5-turbo")
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_model(openrouter_handle_t handle, const char *model);

/**
 * @brief Set the system role message
 *
 * @param handle OpenRouter handle
 * @param role System role message (NULL to clear)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_system_role(openrouter_handle_t handle, const char *role);

/**
 * @brief Set temperature parameter
 *
 * @param handle OpenRouter handle
 * @param temperature Temperature value (0.0-2.0)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_temperature(openrouter_handle_t handle, float temperature);

/**
 * @brief Set maximum tokens to generate
 *
 * @param handle OpenRouter handle
 * @param max_tokens Maximum tokens value
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_max_tokens(openrouter_handle_t handle, int max_tokens);

/**
 * @brief Set top_p (nucleus sampling) parameter
 *
 * @param handle OpenRouter handle
 * @param top_p Top_p value (0.0-1.0)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_top_p(openrouter_handle_t handle, float top_p);

/**
 * @brief Set random seed for deterministic responses
 *
 * @param handle OpenRouter handle
 * @param seed Seed value (-1 for random)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_seed(openrouter_handle_t handle, int seed);

/**
 * @brief Enable or disable streaming mode
 *
 * @param handle OpenRouter handle
 * @param enable_streaming true to enable streaming, false to disable
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_streaming(openrouter_handle_t handle, bool enable_streaming);

/* Function/Tool management functions */

/**
 * @brief Register a function that can be called by the AI
 *
 * @param handle OpenRouter handle
 * @param function Function definition structure
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_register_function(openrouter_handle_t handle, const openrouter_function_t *function);

/**
 * @brief Register a function with simplified parameter definition
 *
 * This is a user-friendly alternative to openrouter_register_function that
 * handles JSON schema creation internally.
 *
 * @param handle OpenRouter handle
 * @param function Simplified function definition structure
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_register_simple_function(openrouter_handle_t handle, const openrouter_simple_function_t *function);

/**
 * @brief Unregister a function by name
 *
 * @param handle OpenRouter handle
 * @param function_name Name of the function to unregister
 * @return esp_err_t ESP_OK on success, ESP_ERR_NOT_FOUND if function not found
 */
esp_err_t openrouter_unregister_function(openrouter_handle_t handle, const char *function_name);

/**
 * @brief Clear all registered functions
 *
 * @param handle OpenRouter handle
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_clear_functions(openrouter_handle_t handle);

/**
 * @brief Enable or disable tools/function calling
 *
 * @param handle OpenRouter handle
 * @param enable_tools true to enable tools, false to disable
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_tools_enabled(openrouter_handle_t handle, bool enable_tools);

/**
 * @brief Set the tool callback function
 *
 * @param handle OpenRouter handle
 * @param callback Tool callback function
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_tool_callback(openrouter_handle_t handle, openrouter_tool_callback_t callback,
                                       void *user_data);

/**
 * @brief Enable or disable HTTP connection reuse
 *
 * @param handle OpenRouter handle
 * @param enable_reuse true to enable connection reuse, false to disable
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_connection_reuse(openrouter_handle_t handle, bool enable_reuse);

/**
 * @brief Set HTTP connection timeout for keep-alive connections
 *
 * @param handle OpenRouter handle
 * @param timeout_ms Timeout in milliseconds (0 to use default)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_connection_timeout(openrouter_handle_t handle, uint32_t timeout_ms);

/**
 * @brief Force close and cleanup any cached HTTP connections
 *
 * @param handle OpenRouter handle
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_close_connection(openrouter_handle_t handle);

/* API call functions - Non-streaming */

/**
 * @brief Make a simple API call with just a prompt
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt User prompt text
 * @param response Buffer to store response
 * @param response_size Size of response buffer
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call(openrouter_handle_t handle, const char *prompt, char *response, size_t response_size);

/**
 * @brief Make an advanced API call with custom messages array
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is configured for streaming
 *
 * @param handle OpenRouter handle
 * @param messages JSON array of message objects
 * @param response Buffer to store response
 * @param response_size Size of response buffer
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_ex(openrouter_handle_t handle, cJSON *messages, char *response, size_t response_size);

/* API call functions - Streaming */

/**
 * @brief Make a simple streaming API call with just a prompt
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is not configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt User prompt text
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_streaming(openrouter_handle_t handle, const char *prompt,
                                    openrouter_stream_callback_t callback, void *user_data);

/**
 * @brief Make an advanced streaming API call with custom messages array
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is not configured for streaming
 *
 * @param handle OpenRouter handle
 * @param messages JSON array of message objects
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_ex_streaming(openrouter_handle_t handle, cJSON *messages,
                                       openrouter_stream_callback_t callback, void *user_data);

/* API call functions with tool support */

/**
 * @brief Make an API call with tool support and automatic tool execution
 *
 * This function automatically handles tool calls by executing registered functions
 * and continuing the conversation with the results.
 *
 * @param handle OpenRouter handle (must have tools enabled)
 * @param prompt User prompt text
 * @param response Buffer to store final response
 * @param response_size Size of response buffer
 * @param max_tool_iterations Maximum number of tool call iterations (0 for unlimited, recommended: 5-10)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_tools(openrouter_handle_t handle, const char *prompt, char *response,
                                     size_t response_size, int max_tool_iterations);

/**
 * @brief Make an advanced API call with tool support and automatic tool execution
 *
 * This function automatically handles tool calls by executing registered functions
 * and continuing the conversation with the results.
 *
 * @param handle OpenRouter handle (must have tools enabled)
 * @param messages JSON array of message objects
 * @param response Buffer to store final response
 * @param response_size Size of response buffer
 * @param max_tool_iterations Maximum number of tool call iterations (0 for unlimited, recommended: 5-10)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_ex_with_tools(openrouter_handle_t handle, cJSON *messages, char *response,
                                        size_t response_size, int max_tool_iterations);

/**
 * @brief Make a streaming API call with tool support
 *
 * When tool calls are received, the registered tool callback will be invoked.
 * The user is responsible for handling tool execution and continuing the conversation.
 *
 * @param handle OpenRouter handle (must have tools enabled and streaming)
 * @param prompt User prompt text
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_streaming_with_tools(openrouter_handle_t handle, const char *prompt,
                                               openrouter_stream_callback_t callback, void *user_data);

/**
 * @brief Make an advanced streaming API call with tool support
 *
 * When tool calls are received, the registered tool callback will be invoked.
 * The user is responsible for handling tool execution and continuing the conversation.
 *
 * @param handle OpenRouter handle (must have tools enabled and streaming)
 * @param messages JSON array of message objects
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_ex_streaming_with_tools(openrouter_handle_t handle, cJSON *messages,
                                                  openrouter_stream_callback_t callback, void *user_data);

/* ================================================================================
 * MULTIMODAL API FUNCTIONS
 * ================================================================================ */

/* Image processing functions - Non-streaming */

/**
 * @brief Make an API call with an image from file path
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the image
 * @param image_path Path to image file (JPEG or PNG)
 * @param response Buffer to store response
 * @param response_size Size of response buffer
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_image(openrouter_handle_t handle, const char *prompt, const char *image_path,
                                     char *response, size_t response_size);

/**
 * @brief Make an API call with an image from URL
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the image
 * @param image_url URL to image file (JPEG or PNG)
 * @param response Buffer to store response
 * @param response_size Size of response buffer
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_image_url(openrouter_handle_t handle, const char *prompt, const char *image_url,
                                         char *response, size_t response_size);

/**
 * @brief Make an API call with image data in memory
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the image
 * @param image_data Pointer to image data in memory
 * @param image_size Size of image data in bytes
 * @param response Buffer to store response
 * @param response_size Size of response buffer
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_image_data(openrouter_handle_t handle, const char *prompt,
                                          const unsigned char *image_data, size_t image_size,
                                          char *response, size_t response_size);

/* Audio processing functions - Non-streaming */

/**
 * @brief Make an API call with an audio file from file path
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the audio
 * @param audio_path Path to audio file (MP3 or WAV)
 * @param response Buffer to store response
 * @param response_size Size of response buffer
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_audio(openrouter_handle_t handle, const char *prompt, const char *audio_path,
                                     char *response, size_t response_size);

/**
 * @brief Make an API call with an audio file from URL
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the audio
 * @param audio_url URL to audio file (MP3 or WAV)
 * @param response Buffer to store response
 * @param response_size Size of response buffer
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_audio_url(openrouter_handle_t handle, const char *prompt, const char *audio_url,
                                         char *response, size_t response_size);

/**
 * @brief Make an API call with audio data in memory
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the audio
 * @param audio_data Pointer to audio data in memory
 * @param audio_size Size of audio data in bytes
 * @param response Buffer to store response
 * @param response_size Size of response buffer
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_audio_data(openrouter_handle_t handle, const char *prompt,
                                          const unsigned char *audio_data, size_t audio_size,
                                          char *response, size_t response_size);

/* Combined media processing - Non-streaming */

/**
 * @brief Make an API call with both image and audio files
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the media
 * @param image_path Path to image file (NULL if no image)
 * @param audio_path Path to audio file (NULL if no audio)
 * @param response Buffer to store response
 * @param response_size Size of response buffer
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_media(openrouter_handle_t handle, const char *prompt,
                                     const char *image_path, const char *audio_path,
                                     char *response, size_t response_size);

/* Image processing functions - Streaming */

/**
 * @brief Make a streaming API call with an image from file path
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is not configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the image
 * @param image_path Path to image file (JPEG or PNG)
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_image_streaming(openrouter_handle_t handle, const char *prompt, const char *image_path,
                                               openrouter_stream_callback_t callback, void *user_data);

/**
 * @brief Make a streaming API call with an image from URL
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is not configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the image
 * @param image_url URL to image file (JPEG or PNG)
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_image_url_streaming(openrouter_handle_t handle, const char *prompt, const char *image_url,
                                                   openrouter_stream_callback_t callback, void *user_data);

/**
 * @brief Make a streaming API call with image data in memory
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is not configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the image
 * @param image_data Pointer to image data in memory
 * @param image_size Size of image data in bytes
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_image_data_streaming(openrouter_handle_t handle, const char *prompt,
                                                    const unsigned char *image_data, size_t image_size,
                                                    openrouter_stream_callback_t callback, void *user_data);

/* Audio processing functions - Streaming */

/**
 * @brief Make a streaming API call with an audio file from file path
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is not configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the audio
 * @param audio_path Path to audio file (MP3 or WAV)
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_audio_streaming(openrouter_handle_t handle, const char *prompt, const char *audio_path,
                                               openrouter_stream_callback_t callback, void *user_data);

/**
 * @brief Make a streaming API call with an audio file from URL
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is not configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the audio
 * @param audio_url URL to audio file (MP3 or WAV)
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_audio_url_streaming(openrouter_handle_t handle, const char *prompt, const char *audio_url,
                                                   openrouter_stream_callback_t callback, void *user_data);

/**
 * @brief Make a streaming API call with audio data in memory
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is not configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the audio
 * @param audio_data Pointer to audio data in memory
 * @param audio_size Size of audio data in bytes
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_audio_data_streaming(openrouter_handle_t handle, const char *prompt,
                                                    const unsigned char *audio_data, size_t audio_size,
                                                    openrouter_stream_callback_t callback, void *user_data);

/* Combined media processing - Streaming */

/**
 * @brief Make a streaming API call with both image and audio files
 *
 * @note This function will return ESP_ERR_INVALID_STATE if handle is not configured for streaming
 *
 * @param handle OpenRouter handle
 * @param prompt Text prompt to accompany the media
 * @param image_path Path to image file (NULL if no image)
 * @param audio_path Path to audio file (NULL if no audio)
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_with_media_streaming(openrouter_handle_t handle, const char *prompt,
                                               const char *image_path, const char *audio_path,
                                               openrouter_stream_callback_t callback, void *user_data);

#ifdef __cplusplus
}
#endif

#endif // OPENROUTER_H