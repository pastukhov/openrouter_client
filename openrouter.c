/* ================================================================================
 * OPENROUTER ESP-IDF CLIENT LIBRARY
 *
 * A comprehensive OpenRouter API client for ESP32 microcontrollers
 * Supports streaming, function calling, and advanced API features
 *
 * ================================================================================ */

#include "openrouter.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_crt_bundle.h"
#endif

/* ================================================================================
 * CONSTANTS AND CONFIGURATION
 * ================================================================================ */

/* Log tag for this module */
static const char *TAG = "OPENROUTER";

/* OpenRouter API URL */
#define API_URL "https://openrouter.ai/api/v1/chat/completions"

/* Initial buffer sizes */
#define INITIAL_RESPONSE_BUFFER_SIZE 1024
#define INITIAL_LINE_BUFFER_SIZE 2048

/* Buffer growth and padding constants */
#define BUFFER_GROWTH_PADDING 512
#define BUFFER_GROWTH_FACTOR 1.5f

/* Maximum lengths for strings */
#define MAX_API_KEY_LENGTH 256
#define MAX_MODEL_NAME_LENGTH 128
#define MAX_SYSTEM_ROLE_LENGTH 2048
#define MAX_RESPONSE_SIZE (1024 * 1024) /* 1MB max response size */
#define MAX_FUNCTION_NAME_LENGTH 64
#define MAX_FUNCTION_DESCRIPTION_LENGTH 512
#define MAX_REGISTERED_FUNCTIONS 32

/* Default retry configuration */
#define DEFAULT_MAX_RETRY_ATTEMPTS 3
#define DEFAULT_RETRY_DELAY_MS 1000
#define DEFAULT_MAX_RETRY_DELAY_MS 32000

/* Static buffer sizes for critical operations */
#define STATIC_AUTH_HEADER_SIZE 512
#define STATIC_LOG_BUFFER_SIZE 256

/* Custom error codes */
#define ESP_ERR_OPENROUTER_BASE 0x9000
#define ESP_ERR_OPENROUTER_MEMORY (ESP_ERR_OPENROUTER_BASE + 1)
#define ESP_ERR_OPENROUTER_JSON_PARSE (ESP_ERR_OPENROUTER_BASE + 2)
#define ESP_ERR_OPENROUTER_API_ERROR (ESP_ERR_OPENROUTER_BASE + 3)
#define ESP_ERR_OPENROUTER_HTTP_ERROR (ESP_ERR_OPENROUTER_BASE + 4)
#define ESP_ERR_OPENROUTER_FUNCTION_EXISTS (ESP_ERR_OPENROUTER_BASE + 5)
#define ESP_ERR_OPENROUTER_FUNCTION_NOT_FOUND (ESP_ERR_OPENROUTER_BASE + 6)
#define ESP_ERR_OPENROUTER_TOOLS_DISABLED (ESP_ERR_OPENROUTER_BASE + 7)
#define ESP_ERR_OPENROUTER_MAX_FUNCTIONS (ESP_ERR_OPENROUTER_BASE + 8)

/* HTTP error codes that might not be defined in older ESP-IDF versions */
#ifndef ESP_ERR_HTTP_CONNECT
#define ESP_ERR_HTTP_CONNECT ESP_ERR_HTTP_BASE
#endif

#ifndef ESP_ERR_HTTP_CONNECTING
#define ESP_ERR_HTTP_CONNECTING ESP_ERR_HTTP_BASE
#endif

#ifndef ESP_ERR_HTTP_CONNECTION_CLOSED
#define ESP_ERR_HTTP_CONNECTION_CLOSED ESP_ERR_HTTP_BASE
#endif

#ifndef ESP_ERR_HTTP_FETCH_HEADER
#define ESP_ERR_HTTP_FETCH_HEADER ESP_ERR_HTTP_BASE
#endif

#ifndef ESP_ERR_HTTP_TIMEOUT
#define ESP_ERR_HTTP_TIMEOUT ESP_ERR_TIMEOUT
#endif

/* ================================================================================
 * MACROS AND UTILITY DEFINITIONS
 * ================================================================================ */

/* Unified error reporting macros */
#define LOG_AND_RETURN_ERROR(err, msg, ...)                                                                            \
    do {                                                                                                               \
        ESP_LOGE(TAG, msg, ##__VA_ARGS__);                                                                             \
        return (err);                                                                                                  \
    } while (0)

#define LOG_AND_GOTO_ERROR(label, msg, ...)                                                                            \
    do {                                                                                                               \
        ESP_LOGE(TAG, msg, ##__VA_ARGS__);                                                                             \
        goto label;                                                                                                    \
    } while (0)

#define CHECK_NULL_AND_RETURN(ptr, err)                                                                                \
    do {                                                                                                               \
        if (!(ptr)) {                                                                                                  \
            ESP_LOGE(TAG, "Null pointer: %s", #ptr);                                                                   \
            return (err);                                                                                              \
        }                                                                                                              \
    } while (0)

#define CHECK_NULL_AND_GOTO(ptr, label)                                                                                \
    do {                                                                                                               \
        if (!(ptr)) {                                                                                                  \
            ESP_LOGE(TAG, "Null pointer: %s", #ptr);                                                                   \
            goto label;                                                                                                \
        }                                                                                                              \
    } while (0)

/* Base64 encoding constants */
#define BASE64_ENCODE_OUT_SIZE(s) ((unsigned int) ((((s) + 2) / 3) * 4 + 1))
#define BASE64_ENCODE_IN_SIZE(s) ((unsigned int) (((s) / 4) * 3))

/* File size limits */
#define MAX_IMAGE_FILE_SIZE (10 * 1024 * 1024) /* 10MB */
#define MAX_AUDIO_FILE_SIZE (25 * 1024 * 1024) /* 25MB */

/* Buffer sizes */
#define URL_BUFFER_SIZE 512
#define MIME_TYPE_BUFFER_SIZE 64
#define DATA_URL_PREFIX_SIZE 128

/* Supported file extensions and MIME types */
static const struct {
    const char *extension;
    const char *mime_type;
} image_types[] = {{".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".png", "image/png"}, {NULL, NULL}};

static const struct {
    const char *extension;
    const char *mime_type;
} audio_types[] = {{".mp3", "audio/mpeg"}, {".wav", "audio/wav"}, {NULL, NULL}};

/**
 * @brief Base64 encoding table
 */
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


/* ================================================================================
 * TYPE DEFINITIONS
 * ================================================================================ */

/**
 * @brief Registered function entry
 */
typedef struct {
    char *name;                              /**< Function name */
    char *description;                       /**< Function description */
    cJSON *parameters;                       /**< Function parameters schema */
    openrouter_function_callback_t callback; /**< Function callback */
    void *user_data;                         /**< User data for callback */
} registered_function_t;

/**
 * @brief HTTP connection pool entry
 */
typedef struct {
    esp_http_client_handle_t client; /**< HTTP client handle */
    uint32_t last_used_time;         /**< Last time this connection was used (in ticks) */
    bool in_use;                     /**< Whether connection is currently being used */
    uint32_t request_count;          /**< Number of requests made on this connection */
} http_connection_pool_t;

/**
 * @brief OpenRouter client handle structure
 */
struct openrouter_handle {
    char *api_key;                                             /**< API key for authentication */
    char *model;                                               /**< Model name to use */
    char *system_role;                                         /**< System role message */
    float temperature;                                         /**< Temperature parameter */
    int max_tokens;                                            /**< Maximum tokens to generate */
    float top_p;                                               /**< Top_p (nucleus sampling) parameter */
    int seed;                                                  /**< Random seed */
    size_t response_buffer_size;                               /**< Response buffer size */
    bool enable_streaming;                                     /**< Streaming mode flag */
    uint32_t http_timeout_ms;                                  /**< HTTP timeout in milliseconds */
    char *http_referer;                                        /**< HTTP-Referer header value */
    char *x_title;                                             /**< X-Title header value */
    bool enable_tools;                                         /**< Tools/function calling enabled flag */
    openrouter_tool_callback_t tool_callback;                  /**< Tool callback function */
    void *tool_user_data;                                      /**< User data for tool callback */
    registered_function_t functions[MAX_REGISTERED_FUNCTIONS]; /**< Registered functions */
    int function_count;                                        /**< Number of registered functions */
    SemaphoreHandle_t mutex;                                   /**< Mutex for thread safety */

    /* HTTP connection reuse configuration */
    bool enable_connection_reuse;           /**< Enable HTTP connection reuse */
    uint32_t connection_timeout_ms;         /**< Connection keep-alive timeout */
    http_connection_pool_t connection_pool; /**< HTTP connection pool */

    /* Configurable retry parameters */
    int max_retry_attempts;      /**< Maximum retry attempts */
    uint32_t retry_delay_ms;     /**< Base retry delay */
    uint32_t max_retry_delay_ms; /**< Maximum retry delay */
};

/**
 * @brief Response data structure for non-streaming mode
 */
typedef struct {
    char *buffer; /**< Response buffer */
    size_t size;  /**< Total buffer size */
    size_t pos;   /**< Current position in buffer */
} response_data_t;

/**
 * @brief Configuration snapshot for thread-safe API calls
 */
typedef struct {
    char *api_key;                            /**< API key copy */
    char *model;                              /**< Model name copy */
    char *system_role;                        /**< System role copy */
    float temperature;                        /**< Temperature parameter */
    int max_tokens;                           /**< Maximum tokens to generate */
    float top_p;                              /**< Top_p parameter */
    int seed;                                 /**< Random seed */
    bool enable_streaming;                    /**< Streaming mode flag */
    uint32_t http_timeout_ms;                 /**< HTTP timeout */
    char *http_referer;                       /**< HTTP-Referer header copy */
    char *x_title;                            /**< X-Title header copy */
    bool enable_tools;                        /**< Tools enabled flag */
    openrouter_tool_callback_t tool_callback; /**< Tool callback copy */
    void *tool_user_data;                     /**< Tool user data copy */
    cJSON *tools_array;                       /**< Tools array for API */
    int max_retry_attempts;                   /**< Maximum retry attempts */
    uint32_t retry_delay_ms;                  /**< Base retry delay */
    uint32_t max_retry_delay_ms;              /**< Maximum retry delay */
} config_snapshot_t;

/**
 * @brief Response data structure for streaming mode
 */
typedef struct {
    char *buffer;                          /**< Buffer to accumulate the full response */
    size_t size;                           /**< Total buffer size */
    size_t pos;                            /**< Current position in buffer */
    openrouter_stream_callback_t callback; /**< User callback function */
    void *user_data;                       /**< User data for callback */
    char *line_buffer;                     /**< Buffer for incomplete lines */
    size_t line_buffer_size;               /**< Line buffer size */
    size_t line_buffer_pos;                /**< Current position in line buffer */
    bool done_sent;                        /**< Track if completion callback was sent */
    char *accumulated_data;                /**< Buffer for accumulating multi-line data events */
    size_t accumulated_data_size;          /**< Size of accumulated data buffer */
    size_t accumulated_data_pos;           /**< Position in accumulated data buffer */
} streaming_response_data_t;

/* ================================================================================
 * UTILITY FUNCTIONS
 * ================================================================================ */


/**
 * @brief Calculate retry delay with exponential backoff and jitter
 *
 * @param attempt_count Current attempt number (0-based)
 * @param base_delay_ms Base delay in milliseconds
 * @param max_delay_ms Maximum delay in milliseconds
 * @return uint32_t Delay in milliseconds
 */
static uint32_t calculate_retry_delay(int attempt_count, uint32_t base_delay_ms, uint32_t max_delay_ms)
{
    /* Prevent overflow: cap attempt_count to reasonable value */
    if (attempt_count > 10) {
        attempt_count = 10;
    }

    /* Exponential backoff: delay = base_delay * 2^attempt_count */
    uint32_t delay = base_delay_ms;
    for (int i = 0; i < attempt_count && delay <= max_delay_ms / 2; i++) {
        delay *= 2;
    }

    /* Cap at maximum delay */
    if (delay > max_delay_ms) {
        delay = max_delay_ms;
    }

    /* Add jitter (up to 25% of delay) with bounds checking */
    uint32_t max_jitter = delay / 4;
    if (max_jitter > 0) {
        uint32_t jitter = esp_random() % max_jitter;
        /* Ensure we don't overflow when adding jitter */
        if (delay <= UINT32_MAX - jitter) {
            delay += jitter;
        }
    }

    return delay;
}

/**
 * @brief Parse Retry-After header value
 *
 * @param client HTTP client handle
 * @return uint32_t Delay in milliseconds, 0 if invalid or header not found
 */
static uint32_t parse_retry_after_header(esp_http_client_handle_t client)
{
    if (!client) {
        return 0;
    }

    // Get the Retry-After header value
    char *retry_after_header = NULL;
    esp_err_t err = esp_http_client_get_header(client, "Retry-After", &retry_after_header);
    if (err != ESP_OK || !retry_after_header) {
        return 0;
    }

    // Parse as seconds
    int seconds = atoi(retry_after_header);
    if (seconds > 0 && seconds <= 300) { // Cap at 5 minutes
        return seconds * 1000;           // Convert to milliseconds
    }

    return 0;
}


/**
 * @brief Centralized string duplication with null checks
 *
 * @param src Source string to duplicate
 * @return char* Duplicated string or NULL on failure
 */
static inline char *safe_strdup(const char *src)
{
    if (!src) {
        return NULL;
    }

    /* Use strnlen for safety to prevent reading beyond reasonable bounds */
    size_t len = strnlen(src, MAX_SYSTEM_ROLE_LENGTH + 1);
    if (len > MAX_SYSTEM_ROLE_LENGTH) {
        ESP_LOGE(TAG, "String too long for duplication (max %d characters)", MAX_SYSTEM_ROLE_LENGTH);
        return NULL;
    }

    /* Allow empty strings to be duplicated */

    char *dst = malloc(len + 1);
    if (!dst) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes for string duplication", len + 1);
        return NULL;
    }

    memcpy(dst, src, len + 1);
    return dst;
}

/**
 * @brief Centralized buffer growth with safety checks
 *
 * @param buffer Pointer to buffer pointer
 * @param current_size Current buffer size
 * @param needed_size Required size
 * @param max_size Maximum allowed size
 * @return esp_err_t ESP_OK on success, error code on failure
 */
static esp_err_t grow_buffer(char **buffer, size_t *current_size, size_t needed_size, size_t max_size)
{
    if (!buffer || !current_size) {
        return ESP_ERR_INVALID_ARG;
    }

    if (needed_size <= *current_size) {
        return ESP_OK; /* No growth needed */
    }

    /* Calculate new size with growth factor and padding */
    size_t new_size = (*current_size > 0) ? *current_size : INITIAL_RESPONSE_BUFFER_SIZE;
    while (new_size < needed_size && new_size <= max_size / BUFFER_GROWTH_FACTOR) {
        new_size = (size_t) (new_size * BUFFER_GROWTH_FACTOR);
    }

    /* Ensure we meet the required size */
    if (new_size < needed_size) {
        new_size = needed_size + BUFFER_GROWTH_PADDING;
    }

    /* Cap at maximum size */
    if (new_size > max_size) {
        new_size = max_size;
        if (needed_size > new_size) {
            ESP_LOGE(TAG, "Required size %zu exceeds maximum allowed size %zu", needed_size, max_size);
            return ESP_ERR_NO_MEM;
        }
    }

    char *new_buffer = realloc(*buffer, new_size);
    if (!new_buffer) {
        ESP_LOGE(TAG, "Failed to reallocate buffer from %zu to %zu bytes", *current_size, new_size);
        return ESP_ERR_NO_MEM;
    }

    *buffer = new_buffer;
    *current_size = new_size;

    ESP_LOGD(TAG, "Expanded buffer from %zu to %zu bytes", *current_size, new_size);
    return ESP_OK;
}

/**
 * @brief Centralized function cleanup helper
 *
 * @param func Pointer to registered function structure
 */
static void cleanup_registered_function(registered_function_t *func)
{
    if (!func) {
        return;
    }

    if (func->name) {
        free(func->name);
        func->name = NULL;
    }

    if (func->description) {
        free(func->description);
        func->description = NULL;
    }

    if (func->parameters) {
        cJSON_Delete(func->parameters);
        func->parameters = NULL;
    }

    func->callback = NULL;
    func->user_data = NULL;
}

/**
 * @brief Common HTTP data processing for both streaming and non-streaming
 *
 * @param buffer Target buffer
 * @param buffer_size Buffer size pointer
 * @param buffer_pos Buffer position pointer
 * @param data Incoming data
 * @param data_len Data length
 * @param max_size Maximum allowed buffer size
 * @return esp_err_t ESP_OK on success, error code on failure
 */
static inline esp_err_t process_http_data(char **buffer, size_t *buffer_size, size_t *buffer_pos, const char *data,
                                          size_t data_len, size_t max_size)
{
    if (!buffer || !buffer_size || !buffer_pos || !data || data_len == 0) {
        return ESP_OK;
    }

    /* Calculate needed size with null terminator */
    size_t needed_size = *buffer_pos + data_len + 1;

    /* Grow buffer if necessary */
    esp_err_t err = grow_buffer(buffer, buffer_size, needed_size, max_size);
    if (err != ESP_OK) {
        return err;
    }

    /* Copy data and null terminate */
    memcpy(*buffer + *buffer_pos, data, data_len);
    *buffer_pos += data_len;
    (*buffer)[*buffer_pos] = '\0';

    return ESP_OK;
}

/* ================================================================================
 * CONNECTION MANAGEMENT FUNCTIONS
 * ================================================================================ */

/**
 * @brief Check if a cached connection is still valid and usable
 *
 * @param handle OpenRouter handle
 * @return true if connection is valid and can be reused, false otherwise
 */
static bool is_connection_valid(openrouter_handle_t handle)
{
    if (!handle || !handle->enable_connection_reuse || !handle->connection_pool.client) {
        return false;
    }

    if (handle->connection_pool.in_use) {
        return false;
    }

    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t elapsed_time = current_time - handle->connection_pool.last_used_time;

    if (elapsed_time > handle->connection_timeout_ms) {
        ESP_LOGD(TAG, "Connection expired after %lu ms (timeout: %lu ms)", elapsed_time, handle->connection_timeout_ms);
        return false;
    }

    return true;
}

/**
 * @brief Cleanup and close a cached HTTP connection
 *
 * @param handle OpenRouter handle
 */
static void cleanup_connection(openrouter_handle_t handle)
{
    if (!handle || !handle->connection_pool.client) {
        return;
    }

    ESP_LOGD(TAG, "Cleaning up HTTP connection (requests served: %lu)", handle->connection_pool.request_count);

    esp_http_client_cleanup(handle->connection_pool.client);

    memset(&handle->connection_pool, 0, sizeof(http_connection_pool_t));
}

/**
 * @brief Get or create an HTTP client for API requests
 *
 * @param handle OpenRouter handle
 * @param config HTTP client configuration
 * @param client_out Pointer to store the HTTP client handle
 * @param is_reused_out Pointer to store whether connection was reused (optional)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
static esp_err_t get_http_client(openrouter_handle_t handle, esp_http_client_config_t *config,
                                 esp_http_client_handle_t *client_out, bool *is_reused_out)
{
    CHECK_NULL_AND_RETURN(handle, ESP_ERR_INVALID_ARG);
    CHECK_NULL_AND_RETURN(config, ESP_ERR_INVALID_ARG);
    CHECK_NULL_AND_RETURN(client_out, ESP_ERR_INVALID_ARG);

    bool is_reused = false;

    /* Check if we can reuse an existing connection */
    if (is_connection_valid(handle)) {
        *client_out = handle->connection_pool.client;
        handle->connection_pool.in_use = true;
        handle->connection_pool.request_count++;
        is_reused = true;

        ESP_LOGD(TAG, "Reusing HTTP connection (request #%lu)", handle->connection_pool.request_count);
    } else {
        /* Clean up any old connection */
        if (handle->connection_pool.client) {
            cleanup_connection(handle);
        }

        /* Create new connection */
        esp_http_client_handle_t new_client = esp_http_client_init(config);
        if (!new_client) {
            LOG_AND_RETURN_ERROR(ESP_ERR_NO_MEM, "Failed to initialize HTTP client");
        }

        *client_out = new_client;

        /* Store in connection pool if reuse is enabled */
        if (handle->enable_connection_reuse) {
            handle->connection_pool.client = new_client;
            handle->connection_pool.last_used_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            handle->connection_pool.in_use = true;
            handle->connection_pool.request_count = 1;

            ESP_LOGD(TAG, "Created new HTTP connection for reuse");
        } else {
            ESP_LOGD(TAG, "Created new HTTP connection (reuse disabled)");
        }
    }

    if (is_reused_out) {
        *is_reused_out = is_reused;
    }

    return ESP_OK;
}

/**
 * @brief Release an HTTP client back to the pool or cleanup if not reusing
 *
 * @param handle OpenRouter handle
 * @param client HTTP client handle
 */
static void release_http_client(openrouter_handle_t handle, esp_http_client_handle_t client)
{
    if (!handle || !client) {
        return;
    }

    if (handle->enable_connection_reuse && handle->connection_pool.client == client) {
        /* Mark as not in use but keep the connection alive */
        handle->connection_pool.in_use = false;
        handle->connection_pool.last_used_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        ESP_LOGD(TAG, "Released HTTP connection back to pool");
    } else {
        /* Not a pooled connection, clean it up immediately */
        esp_http_client_cleanup(client);

        ESP_LOGD(TAG, "Cleaned up non-pooled HTTP connection");
    }
}

/* ================================================================================
 * HTTP EVENT HANDLERS
 * ================================================================================ */

/**
 * @brief HTTP event handler for non-streaming responses
 *
 * @param evt HTTP client event
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
static esp_err_t http_event_handler_non_streaming(esp_http_client_event_t *evt)
{
    CHECK_NULL_AND_RETURN(evt, ESP_ERR_INVALID_ARG);
    CHECK_NULL_AND_RETURN(evt->user_data, ESP_ERR_INVALID_ARG);

    response_data_t *resp_data = (response_data_t *) evt->user_data;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA (non-streaming): %.*s", evt->data_len, (char *) evt->data);
#endif
        return process_http_data(&resp_data->buffer, &resp_data->size, &resp_data->pos, evt->data, evt->data_len,
                                 MAX_RESPONSE_SIZE);

    case HTTP_EVENT_ON_FINISH:
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH (non-streaming) - Total received: %zu bytes", resp_data->pos);
#endif
        break;

    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "HTTP_EVENT_DISCONNECTED (non-streaming)");
        break;

#ifdef HTTP_EVENT_REDIRECT
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
        break;
#endif

    default:
        ESP_LOGV(TAG, "Unhandled HTTP event: %d", evt->event_id);
        break;
    }
    return ESP_OK;
}

// HTTP event handler for streaming responses
static esp_err_t http_event_handler_streaming(esp_http_client_event_t *evt)
{
    CHECK_NULL_AND_RETURN(evt, ESP_ERR_INVALID_ARG);
    CHECK_NULL_AND_RETURN(evt->user_data, ESP_ERR_INVALID_ARG);

    streaming_response_data_t *resp_data = (streaming_response_data_t *) evt->user_data;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA: {
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA (streaming): %.*s", evt->data_len, (char *) evt->data);
#endif

        // Accumulate data in the full response buffer if enabled
        if (resp_data->buffer != NULL) {
            // Ensure there's enough space for new data
            size_t full_needed_size = resp_data->pos + evt->data_len + 1;
            if (full_needed_size > resp_data->size) {
                // Grow by factor with padding
                size_t new_size = (full_needed_size > resp_data->size * BUFFER_GROWTH_FACTOR)
                                      ? full_needed_size + BUFFER_GROWTH_PADDING
                                      : resp_data->size * BUFFER_GROWTH_FACTOR;

                // Cap at maximum size
                if (new_size > MAX_RESPONSE_SIZE) {
                    new_size = MAX_RESPONSE_SIZE;
                    if (full_needed_size > new_size) {
                        ESP_LOGW(TAG, "Full response buffer exceeded maximum size, truncating");
                        // We'll continue but truncate the response
                    }
                }

                char *new_buffer = realloc(resp_data->buffer, new_size);
                if (new_buffer == NULL) {
                    ESP_LOGW(TAG, "Failed to reallocate full response buffer, continuing without it");
                    // Free existing buffer to prevent memory leaks
                    free(resp_data->buffer);
                    resp_data->buffer = NULL;
                    resp_data->size = 0;
                    resp_data->pos = 0;
                } else {
                    resp_data->buffer = new_buffer;
                    resp_data->size = new_size;
                }
            }

            // Append data if we still have a buffer
            if (resp_data->buffer != NULL) {
                size_t bytes_to_copy = evt->data_len;
                // If we're at the cap, only copy what fits
                if (resp_data->pos + bytes_to_copy > resp_data->size - 1) {
                    bytes_to_copy = resp_data->size - resp_data->pos - 1;
                }

                memcpy(resp_data->buffer + resp_data->pos, evt->data, bytes_to_copy);
                resp_data->pos += bytes_to_copy;
                resp_data->buffer[resp_data->pos] = '\0';
            }
        }

        // Add incoming data to line buffer
        size_t needed_size = resp_data->line_buffer_pos + evt->data_len + 1;
        if (needed_size > resp_data->line_buffer_size) {
            // Grow by factor with padding
            size_t new_size = (needed_size > resp_data->line_buffer_size * BUFFER_GROWTH_FACTOR)
                                  ? needed_size + BUFFER_GROWTH_PADDING
                                  : resp_data->line_buffer_size * BUFFER_GROWTH_FACTOR;

            // Cap at maximum size
            if (new_size > MAX_RESPONSE_SIZE) {
                new_size = MAX_RESPONSE_SIZE;
                if (needed_size > new_size) {
                    ESP_LOGE(TAG, "Line buffer exceeds maximum allowed size (%d bytes)", MAX_RESPONSE_SIZE);
                    if (resp_data->callback && !resp_data->done_sent) {
                        resp_data->callback("[ERROR: Line buffer too large]", true, resp_data->user_data);
                        resp_data->done_sent = true;
                    }
                    return ESP_ERR_OPENROUTER_MEMORY;
                }
            }
            char *new_buffer = realloc(resp_data->line_buffer, new_size);
            if (new_buffer == NULL) {
                ESP_LOGE(TAG, "Failed to reallocate line buffer");
                if (resp_data->callback && !resp_data->done_sent) {
                    resp_data->callback("[ERROR: Memory allocation failed]", true, resp_data->user_data);
                    resp_data->done_sent = true;
                }
                return ESP_ERR_OPENROUTER_MEMORY;
            }
            resp_data->line_buffer = new_buffer;
            resp_data->line_buffer_size = new_size;
        }

        // Append new data
        memcpy(resp_data->line_buffer + resp_data->line_buffer_pos, evt->data, evt->data_len);
        resp_data->line_buffer_pos += evt->data_len;
        resp_data->line_buffer[resp_data->line_buffer_pos] = '\0';

        // Process complete lines
        char *line_start = resp_data->line_buffer;
        char *line_end;

        while ((line_end = strstr(line_start, "\n")) != NULL) {
            *line_end = '\0'; // Null terminate the line

            // Remove \r if present
            size_t line_len = strlen(line_start);
            if (line_len > 0 && line_start[line_len - 1] == '\r') {
                line_start[line_len - 1] = '\0';
            }

            // Process SSE line (Server-Sent Events protocol)
            // SSE format: "data: {json}" or "data:{json}" for each chunk
            // Also handle heartbeat lines that start with ":"
            if (line_start[0] == ':') {
                // Skip heartbeat/comment lines
                ESP_LOGV(TAG, "Skipping SSE heartbeat line");
            } else if (strncmp(line_start, "data:", 5) == 0) {
                char *json_data;
                // Handle both "data: " and "data:" formats
                if (line_start[5] == ' ') {
                    json_data = line_start + 6; // Skip "data: "
                } else {
                    json_data = line_start + 5; // Skip "data:"
                }

                // Accumulate data lines until we get an empty line or [DONE]
                if (strlen(json_data) > 0) {
                    // Ensure accumulated data buffer is large enough
                    size_t json_len = strlen(json_data);
                    size_t needed_size = resp_data->accumulated_data_pos + json_len + 1;

                    if (needed_size > resp_data->accumulated_data_size) {
                        // Grow by factor with padding
                        size_t new_size = (needed_size > resp_data->accumulated_data_size * BUFFER_GROWTH_FACTOR)
                                              ? needed_size + BUFFER_GROWTH_PADDING
                                              : resp_data->accumulated_data_size * BUFFER_GROWTH_FACTOR;

                        // Cap at maximum response size
                        if (new_size > MAX_RESPONSE_SIZE) {
                            new_size = MAX_RESPONSE_SIZE;
                            if (needed_size > new_size) {
                                ESP_LOGE(TAG, "Accumulated data exceeds maximum allowed size (%d bytes)",
                                         MAX_RESPONSE_SIZE);
                                if (resp_data->callback && !resp_data->done_sent) {
                                    resp_data->callback("[ERROR: Response too large]", true, resp_data->user_data);
                                    resp_data->done_sent = true;
                                }
                                return ESP_ERR_OPENROUTER_MEMORY;
                            }
                        }

                        char *new_buffer = realloc(resp_data->accumulated_data, new_size);
                        if (new_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to reallocate accumulated data buffer");
                            if (resp_data->callback && !resp_data->done_sent) {
                                resp_data->callback("[ERROR: Memory allocation failed]", true, resp_data->user_data);
                                resp_data->done_sent = true;
                            }
                            return ESP_ERR_OPENROUTER_MEMORY;
                        }
                        resp_data->accumulated_data = new_buffer;
                        resp_data->accumulated_data_size = new_size;
                    }

                    // Safely append this data line with length checking
                    size_t bytes_to_copy = json_len;
                    if (resp_data->accumulated_data_pos + bytes_to_copy >= resp_data->accumulated_data_size) {
                        bytes_to_copy = resp_data->accumulated_data_size - resp_data->accumulated_data_pos - 1;
                    }

                    if (bytes_to_copy > 0) {
                        memcpy(resp_data->accumulated_data + resp_data->accumulated_data_pos, json_data, bytes_to_copy);
                        resp_data->accumulated_data_pos += bytes_to_copy;
                        resp_data->accumulated_data[resp_data->accumulated_data_pos] = '\0';
                    }
                }
            } else if (strlen(line_start) == 0 && resp_data->accumulated_data_pos > 0) {
                // Empty line - process accumulated data
                char *json_data = resp_data->accumulated_data;

                // Check for end of stream marker
                if (strcmp(json_data, "[DONE]") == 0) {
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
                    ESP_LOGI(TAG, "Stream completed");
#endif
                    if (resp_data->callback && !resp_data->done_sent) {
                        resp_data->callback("", true, resp_data->user_data);
                        resp_data->done_sent = true;
                    }
                    break;
                }

                // Parse JSON chunk
                cJSON *json = cJSON_Parse(json_data);
                if (json) {
                    cJSON *choices = cJSON_GetObjectItem(json, "choices");
                    if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
                        cJSON *choice = cJSON_GetArrayItem(choices, 0);
                        if (choice) {
                            cJSON *delta = cJSON_GetObjectItem(choice, "delta");
                            if (delta) {
                                bool has_content = false;

                                // Handle regular content
                                cJSON *content = cJSON_GetObjectItem(delta, "content");
                                if (content && cJSON_IsString(content) && content->valuestring &&
                                    strlen(content->valuestring) > 0) {
                                    if (resp_data->callback && !resp_data->done_sent) {
                                        resp_data->callback(content->valuestring, false, resp_data->user_data);
                                    }
                                    has_content = true;
                                }

                                // Handle tool_calls deltas
                                cJSON *tool_calls = cJSON_GetObjectItem(delta, "tool_calls");
                                if (tool_calls && cJSON_IsArray(tool_calls)) {
                                    char *tool_calls_str = cJSON_PrintUnformatted(tool_calls);
                                    if (tool_calls_str && resp_data->callback && !resp_data->done_sent) {
                                        char formatted_msg[512];
                                        snprintf(formatted_msg, sizeof(formatted_msg), "[TOOL_CALLS_DELTA: %s]",
                                                 tool_calls_str);
                                        resp_data->callback(formatted_msg, false, resp_data->user_data);
                                    }
                                    if (tool_calls_str)
                                        free(tool_calls_str);
                                    has_content = true;
                                }

                                // Handle function_call deltas (legacy format)
                                cJSON *function_call = cJSON_GetObjectItem(delta, "function_call");
                                if (function_call) {
                                    char *function_call_str = cJSON_PrintUnformatted(function_call);
                                    if (function_call_str && resp_data->callback && !resp_data->done_sent) {
                                        char formatted_msg[512];
                                        snprintf(formatted_msg, sizeof(formatted_msg), "[FUNCTION_CALL_DELTA: %s]",
                                                 function_call_str);
                                        resp_data->callback(formatted_msg, false, resp_data->user_data);
                                    }
                                    if (function_call_str)
                                        free(function_call_str);
                                    has_content = true;
                                }

                                // Log if delta was empty or role-only
                                if (!has_content) {
                                    ESP_LOGV(TAG, "Delta contains no streamable content (role-only or empty)");
                                }
                            } else {
                                ESP_LOGW(TAG, "No delta found in choice");
                            }
                        } else {
                            ESP_LOGW(TAG, "No valid choice found in array");
                        }
                    } else {
                        // Check for error in response
                        cJSON *error = cJSON_GetObjectItem(json, "error");
                        if (error) {
                            cJSON *error_msg = cJSON_GetObjectItem(error, "message");
                            if (error_msg && cJSON_IsString(error_msg) && resp_data->callback &&
                                !resp_data->done_sent) {
                                char err_buf[256];
                                snprintf(err_buf, sizeof(err_buf), "[ERROR: %s]", error_msg->valuestring);
                                resp_data->callback(err_buf, false, resp_data->user_data);
                                ESP_LOGE(TAG, "API Error: %s", error_msg->valuestring);
                            }
                        } else {
                            ESP_LOGW(TAG, "No choices array found or empty");
                        }
                    }
                    cJSON_Delete(json);
                } else {
                    ESP_LOGW(TAG, "Failed to parse JSON: %s", json_data);
                    // Notify callback of error
                    if (resp_data->callback && !resp_data->done_sent) {
                        char error_msg[128];
                        snprintf(error_msg, sizeof(error_msg), "[ERROR: JSON parse error]");
                        resp_data->callback(error_msg, false, resp_data->user_data);
                    }
                }

                // Reset accumulated data for next event
                resp_data->accumulated_data_pos = 0;
                if (resp_data->accumulated_data) {
                    resp_data->accumulated_data[0] = '\0';
                }
            }

            // Move to next line
            line_start = line_end + 1;
        }

        // Move remaining incomplete data to beginning of buffer
        if (line_start < resp_data->line_buffer + resp_data->line_buffer_pos) {
            size_t remaining = resp_data->line_buffer_pos - (line_start - resp_data->line_buffer);
            memmove(resp_data->line_buffer, line_start, remaining);
            resp_data->line_buffer_pos = remaining;
            resp_data->line_buffer[resp_data->line_buffer_pos] = '\0';
        } else {
            resp_data->line_buffer_pos = 0;
            resp_data->line_buffer[0] = '\0';
        }
        break;
    }

    case HTTP_EVENT_ON_FINISH:
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH (streaming)");
#endif
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "HTTP_EVENT_DISCONNECTED (streaming)");
        // Notify callback of disconnection only if we haven't already sent completion
        if (resp_data->callback && !resp_data->done_sent) {
            resp_data->callback("[ERROR: Connection disconnected]", true, resp_data->user_data);
            resp_data->done_sent = true;
        }
        break;

    default:
        break;
    }
    return ESP_OK;
}

/* ================================================================================
 * HANDLE MANAGEMENT
 * ================================================================================ */

/**
 * @brief Create a new OpenRouter client handle
 *
 * Creates a thread-safe OpenRouter client handle with the specified configuration.
 * The handle manages internal resources including mutexes and copied configuration strings.
 * All string parameters in the config are copied internally, so the caller can free them after this call.
 *
 * @param config Pointer to configuration structure containing API key, model, and other settings
 * @return openrouter_handle_t Handle on success, NULL on failure
 *
 * @note The returned handle must be freed with openrouter_destroy() to prevent memory leaks
 * @note Configuration values are validated and defaults applied for missing optional parameters
 */
openrouter_handle_t openrouter_create(const openrouter_config_t *config)
{
    CHECK_NULL_AND_RETURN(config, NULL);
    CHECK_NULL_AND_RETURN(config->api_key, NULL);

    /* Validate string lengths */
    if (strlen(config->api_key) == 0 || strlen(config->api_key) > MAX_API_KEY_LENGTH) {
        LOG_AND_RETURN_ERROR(NULL, "API key is invalid (empty or too long, max %d characters)", MAX_API_KEY_LENGTH);
    }

    if (config->default_model && strlen(config->default_model) > MAX_MODEL_NAME_LENGTH) {
        LOG_AND_RETURN_ERROR(NULL, "Model name is too long (max %d characters)", MAX_MODEL_NAME_LENGTH);
    }

    if (config->default_system_role && strlen(config->default_system_role) > MAX_SYSTEM_ROLE_LENGTH) {
        LOG_AND_RETURN_ERROR(NULL, "System role is too long (max %d characters)", MAX_SYSTEM_ROLE_LENGTH);
    }

    /* Validate numerical parameters */
    if (config->temperature < 0.0f || config->temperature > 2.0f) {
        if (config->temperature != 0.0f) { /* Allow 0.0 as "use default" */
            LOG_AND_RETURN_ERROR(NULL, "Temperature must be between 0.0 and 2.0 (got %.2f)", config->temperature);
        }
    }

    if (config->top_p < 0.0f || config->top_p > 1.0f) {
        if (config->top_p != 0.0f) { /* Allow 0.0 as "use default" */
            LOG_AND_RETURN_ERROR(NULL, "Top_p must be between 0.0 and 1.0 (got %.2f)", config->top_p);
        }
    }

    if (config->max_tokens < 0) {
        LOG_AND_RETURN_ERROR(NULL, "Max tokens cannot be negative (got %d)", config->max_tokens);
    }

    /* Allocate handle structure */
    openrouter_handle_t handle = calloc(1, sizeof(struct openrouter_handle));
    if (!handle) {
        LOG_AND_RETURN_ERROR(NULL, "Failed to allocate handle");
    }

    /* Initialize configurable retry parameters with defaults */
    handle->max_retry_attempts = DEFAULT_MAX_RETRY_ATTEMPTS;
    handle->retry_delay_ms = DEFAULT_RETRY_DELAY_MS;
    handle->max_retry_delay_ms = DEFAULT_MAX_RETRY_DELAY_MS;

    /* Copy API key */
    handle->api_key = safe_strdup(config->api_key);
    if (!handle->api_key) {
        LOG_AND_GOTO_ERROR(cleanup, "Failed to allocate memory for API key");
    }

    /* Set model with default fallback */
    const char *model_to_use = config->default_model ? config->default_model : "openai/gpt-4o-mini";
    handle->model = safe_strdup(model_to_use);
    if (!handle->model) {
        LOG_AND_GOTO_ERROR(cleanup, "Failed to allocate memory for model name");
    }

    /* Set system role if provided */
    if (config->default_system_role) {
        handle->system_role = safe_strdup(config->default_system_role);
        /* Non-critical, continue without system role if allocation fails */
        if (!handle->system_role) {
            ESP_LOGW(TAG, "Failed to allocate memory for system role, continuing without it");
        }
    }

    /* Set optional header fields */
    if (config->http_referer) {
        handle->http_referer = safe_strdup(config->http_referer);
        if (!handle->http_referer) {
            ESP_LOGW(TAG, "Failed to allocate memory for HTTP referer");
        }
    }

    if (config->x_title) {
        handle->x_title = safe_strdup(config->x_title);
        if (!handle->x_title) {
            ESP_LOGW(TAG, "Failed to allocate memory for X-Title header");
        }
    }

    /* Set other parameters with defaults */
    handle->temperature = config->temperature > 0 ? config->temperature : OPENROUTER_DEFAULT_TEMPERATURE;
    handle->max_tokens = config->max_tokens > 0 ? config->max_tokens : OPENROUTER_DEFAULT_MAX_TOKENS;
    handle->top_p = config->top_p > 0 ? config->top_p : OPENROUTER_DEFAULT_TOP_P;
    handle->seed = config->seed;

    /* Validate and set response buffer size */
    size_t buffer_size =
        config->response_buffer_size > 0 ? config->response_buffer_size : OPENROUTER_DEFAULT_RESPONSE_BUFFER_SIZE;
    if (buffer_size < 512) {
        ESP_LOGW(TAG, "Response buffer size %zu too small, using minimum 512 bytes", buffer_size);
        buffer_size = 512;
    }
    handle->response_buffer_size = buffer_size;

    handle->enable_streaming = config->enable_streaming;
    handle->http_timeout_ms =
        config->http_timeout_ms > 0
            ? config->http_timeout_ms
            : (handle->enable_streaming ? OPENROUTER_DEFAULT_HTTP_TIMEOUT_STREAMING : OPENROUTER_DEFAULT_HTTP_TIMEOUT);

    /* Initialize tools/function calling fields */
    handle->enable_tools = config->enable_tools;
    handle->tool_callback = config->tool_callback;
    handle->tool_user_data = config->tool_user_data;
    handle->function_count = 0;
    memset(handle->functions, 0, sizeof(handle->functions));

    /* Initialize HTTP connection reuse configuration */
    handle->enable_connection_reuse = config->enable_connection_reuse;
    handle->connection_timeout_ms =
        config->connection_timeout_ms > 0 ? config->connection_timeout_ms : OPENROUTER_DEFAULT_CONNECTION_TIMEOUT;
    memset(&handle->connection_pool, 0, sizeof(http_connection_pool_t));

    /* Create mutex for thread safety */
    handle->mutex = xSemaphoreCreateMutex();
    if (!handle->mutex) {
        LOG_AND_GOTO_ERROR(cleanup, "Failed to create mutex");
    }

    ESP_LOGI(TAG,
             "OpenRouter client created - model: %s, temp: %.2f, top_p: %.2f, max_tokens: %d, timeout: %" PRIu32
             "ms, streaming: %s, connection_reuse: %s",
             handle->model, handle->temperature, handle->top_p, handle->max_tokens, handle->http_timeout_ms,
             handle->enable_streaming ? "enabled" : "disabled",
             handle->enable_connection_reuse ? "enabled" : "disabled");
    return handle;

cleanup:
    /* Unified cleanup on error */
    if (handle) {
        free(handle->api_key);
        free(handle->model);
        free(handle->system_role);
        free(handle->http_referer);
        free(handle->x_title);
        free(handle);
    }
    return NULL;
}

/**
 * @brief Destroy an OpenRouter client handle and free all resources
 *
 * @param handle OpenRouter handle to destroy
 */
void openrouter_destroy(openrouter_handle_t handle)
{
    if (!handle) {
        return;
    }

    /* Take mutex briefly to ensure no other operations are in progress */
    if (handle->mutex) {
        xSemaphoreTake(handle->mutex, portMAX_DELAY);
    }

    /* Free string fields safely */
    if (handle->api_key) {
        free(handle->api_key);
        handle->api_key = NULL;
    }
    if (handle->model) {
        free(handle->model);
        handle->model = NULL;
    }
    if (handle->system_role) {
        free(handle->system_role);
        handle->system_role = NULL;
    }
    if (handle->http_referer) {
        free(handle->http_referer);
        handle->http_referer = NULL;
    }
    if (handle->x_title) {
        free(handle->x_title);
        handle->x_title = NULL;
    }

    /* Clean up registered functions using centralized helper */
    for (int i = 0; i < handle->function_count; i++) {
        cleanup_registered_function(&handle->functions[i]);
    }
    handle->function_count = 0;

    /* Clean up cached HTTP connection */
    cleanup_connection(handle);

    /* Release and clean up mutex */
    if (handle->mutex) {
        xSemaphoreGive(handle->mutex);
        vSemaphoreDelete(handle->mutex);
        handle->mutex = NULL;
    }

    /* Free the handle itself */
    free(handle);
}

/* ================================================================================
 * CONFIGURATION MANAGEMENT
 * ================================================================================ */

/**
 * @brief Set the model to use for API calls
 *
 * @param handle OpenRouter handle
 * @param model Model name (e.g., "openai/gpt-3.5-turbo")
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_model(openrouter_handle_t handle, const char *model)
{
    CHECK_NULL_AND_RETURN(handle, ESP_ERR_INVALID_ARG);
    CHECK_NULL_AND_RETURN(model, ESP_ERR_INVALID_ARG);

    if (strlen(model) > MAX_MODEL_NAME_LENGTH) {
        LOG_AND_RETURN_ERROR(ESP_ERR_INVALID_ARG, "Model name is too long (max %d characters)", MAX_MODEL_NAME_LENGTH);
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    char *new_model = safe_strdup(model);
    if (!new_model) {
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_OPENROUTER_MEMORY;
    }

    free(handle->model);
    handle->model = new_model;

    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}

/**
 * @brief Set the system role message
 *
 * @param handle OpenRouter handle
 * @param role System role message (NULL to clear)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_system_role(openrouter_handle_t handle, const char *role)
{
    CHECK_NULL_AND_RETURN(handle, ESP_ERR_INVALID_ARG);

    if (role && strlen(role) > MAX_SYSTEM_ROLE_LENGTH) {
        LOG_AND_RETURN_ERROR(ESP_ERR_INVALID_ARG, "System role is too long (max %d characters)",
                             MAX_SYSTEM_ROLE_LENGTH);
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    free(handle->system_role);
    handle->system_role = role ? safe_strdup(role) : NULL;

    if (role && !handle->system_role) {
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_OPENROUTER_MEMORY;
    }

    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}

/**
 * @brief Set temperature parameter
 *
 * Controls the randomness of the model's output. Higher values (e.g., 0.8) produce
 * more diverse and creative outputs, while lower values (e.g., 0.2) produce more
 * focused and deterministic outputs. Range: 0.0-2.0.
 *
 * @param handle OpenRouter handle
 * @param temperature Temperature value (0.0-2.0)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_temperature(openrouter_handle_t handle, float temperature)
{
    CHECK_NULL_AND_RETURN(handle, ESP_ERR_INVALID_ARG);

    if (temperature < 0.0f || temperature > 2.0f) {
        ESP_LOGW(TAG, "Temperature value %f outside recommended range [0.0-2.0]", temperature);
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->temperature = temperature;
    xSemaphoreGive(handle->mutex);

    return ESP_OK;
}

/**
 * @brief Set maximum tokens to generate
 *
 * Limits the maximum number of tokens that can be generated in the response.
 * One token is roughly 4 characters for normal English text.
 *
 * @param handle OpenRouter handle
 * @param max_tokens Maximum tokens value
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_max_tokens(openrouter_handle_t handle, int max_tokens)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    if (max_tokens <= 0) {
        ESP_LOGW(TAG, "max_tokens value %d should be positive", max_tokens);
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->max_tokens = max_tokens;
    xSemaphoreGive(handle->mutex);

    return ESP_OK;
}

/**
 * @brief Set top_p (nucleus sampling) parameter
 *
 * Controls diversity via nucleus sampling: 0.1 means only the tokens comprising
 * the top 10% probability mass are considered. We generally recommend altering
 * this or temperature but not both. Range: 0.0-1.0.
 *
 * @param handle OpenRouter handle
 * @param top_p Top_p value (0.0-1.0)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_top_p(openrouter_handle_t handle, float top_p)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    if (top_p < 0.0f || top_p > 1.0f) {
        ESP_LOGW(TAG, "top_p value %f outside recommended range [0.0-1.0]", top_p);
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->top_p = top_p;
    xSemaphoreGive(handle->mutex);

    return ESP_OK;
}

/**
 * @brief Set random seed for deterministic responses
 *
 * Providing the same seed will generate the same sequence of responses for
 * the same inputs. Use this for reproducible outputs. Set to -1 for random
 * behavior.
 *
 * @param handle OpenRouter handle
 * @param seed Seed value (-1 for random)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_seed(openrouter_handle_t handle, int seed)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->seed = seed;
    xSemaphoreGive(handle->mutex);

    return ESP_OK;
}

/**
 * @brief Enable or disable streaming mode
 *
 * @param handle OpenRouter handle
 * @param enable_streaming true to enable streaming, false to disable
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_streaming(openrouter_handle_t handle, bool enable_streaming)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->enable_streaming = enable_streaming;
    xSemaphoreGive(handle->mutex);

    return ESP_OK;
}

/**
 * @brief Enable or disable HTTP connection reuse
 *
 * @param handle OpenRouter handle
 * @param enable_reuse true to enable connection reuse, false to disable
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_connection_reuse(openrouter_handle_t handle, bool enable_reuse)
{
    CHECK_NULL_AND_RETURN(handle, ESP_ERR_INVALID_ARG);

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    /* If disabling reuse, cleanup any existing connection */
    if (!enable_reuse && handle->enable_connection_reuse) {
        cleanup_connection(handle);
    }

    handle->enable_connection_reuse = enable_reuse;
    xSemaphoreGive(handle->mutex);

    ESP_LOGI(TAG, "HTTP connection reuse %s", enable_reuse ? "enabled" : "disabled");
    return ESP_OK;
}

/**
 * @brief Set HTTP connection timeout for keep-alive connections
 *
 * @param handle OpenRouter handle
 * @param timeout_ms Timeout in milliseconds (0 to use default)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_connection_timeout(openrouter_handle_t handle, uint32_t timeout_ms)
{
    CHECK_NULL_AND_RETURN(handle, ESP_ERR_INVALID_ARG);

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->connection_timeout_ms = timeout_ms > 0 ? timeout_ms : OPENROUTER_DEFAULT_CONNECTION_TIMEOUT;
    xSemaphoreGive(handle->mutex);

    ESP_LOGI(TAG, "HTTP connection timeout set to %lu ms", handle->connection_timeout_ms);
    return ESP_OK;
}

/**
 * @brief Force close and cleanup any cached HTTP connections
 *
 * @param handle OpenRouter handle
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_close_connection(openrouter_handle_t handle)
{
    CHECK_NULL_AND_RETURN(handle, ESP_ERR_INVALID_ARG);

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    cleanup_connection(handle);
    xSemaphoreGive(handle->mutex);

    ESP_LOGI(TAG, "HTTP connection closed and cleaned up");
    return ESP_OK;
}

/* ================================================================================
 * FUNCTION REGISTRY
 * ================================================================================ */

/**
 * @brief Register a function that can be called by the AI
 *
 * @param handle OpenRouter handle
 * @param function Function definition structure
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_register_function(openrouter_handle_t handle, const openrouter_function_t *function)
{
    CHECK_NULL_AND_RETURN(handle, ESP_ERR_INVALID_ARG);
    CHECK_NULL_AND_RETURN(function, ESP_ERR_INVALID_ARG);
    CHECK_NULL_AND_RETURN(function->name, ESP_ERR_INVALID_ARG);
    CHECK_NULL_AND_RETURN(function->callback, ESP_ERR_INVALID_ARG);

    if (strlen(function->name) == 0 || strlen(function->name) > MAX_FUNCTION_NAME_LENGTH) {
        LOG_AND_RETURN_ERROR(ESP_ERR_INVALID_ARG, "Function name is invalid (empty or too long, max %d characters)",
                             MAX_FUNCTION_NAME_LENGTH);
    }

    /* Validate function name characters (alphanumeric, underscore, hyphen) */
    for (const char *p = function->name; *p; p++) {
        if (!isalnum((unsigned char) *p) && *p != '_' && *p != '-') {
            LOG_AND_RETURN_ERROR(ESP_ERR_INVALID_ARG, "Function name contains invalid character: '%c'", *p);
        }
    }

    if (function->description && strlen(function->description) > MAX_FUNCTION_DESCRIPTION_LENGTH) {
        LOG_AND_RETURN_ERROR(ESP_ERR_INVALID_ARG, "Function description is too long (max %d characters)",
                             MAX_FUNCTION_DESCRIPTION_LENGTH);
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    /* Check if function already exists */
    for (int i = 0; i < handle->function_count; i++) {
        if (handle->functions[i].name && strcmp(handle->functions[i].name, function->name) == 0) {
            xSemaphoreGive(handle->mutex);
            return ESP_ERR_OPENROUTER_FUNCTION_EXISTS;
        }
    }

    /* Check if we have space for more functions */
    if (handle->function_count >= MAX_REGISTERED_FUNCTIONS) {
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_OPENROUTER_MAX_FUNCTIONS;
    }

    /* Find empty slot */
    int slot = handle->function_count;
    registered_function_t *reg_func = &handle->functions[slot];

    /* Copy function name */
    reg_func->name = safe_strdup(function->name);
    if (!reg_func->name) {
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_OPENROUTER_MEMORY;
    }

    /* Copy function description */
    if (function->description) {
        reg_func->description = safe_strdup(function->description);
        if (!reg_func->description) {
            cleanup_registered_function(reg_func);
            xSemaphoreGive(handle->mutex);
            return ESP_ERR_OPENROUTER_MEMORY;
        }
    }

    /* Copy parameters schema */
    if (function->parameters) {
        reg_func->parameters = cJSON_Duplicate(function->parameters, true);
        if (!reg_func->parameters) {
            cleanup_registered_function(reg_func);
            xSemaphoreGive(handle->mutex);
            return ESP_ERR_OPENROUTER_MEMORY;
        }
    }

    /* Set callback and user data */
    reg_func->callback = function->callback;
    reg_func->user_data = function->user_data;

    handle->function_count++;

    xSemaphoreGive(handle->mutex);

    ESP_LOGI(TAG, "Registered function: %s", function->name);
    return ESP_OK;
}

/**
 * @brief Create JSON schema from simplified parameter definition
 *
 * @param parameters Array of simplified parameters (NULL-terminated)
 * @return cJSON* JSON schema object, or NULL on failure
 */
static cJSON *create_json_schema_from_params(const openrouter_param_t *parameters)
{
    if (!parameters) {
        return NULL;
    }

    cJSON *schema = cJSON_CreateObject();
    if (!schema) {
        return NULL;
    }

    cJSON_AddStringToObject(schema, "type", "object");
    cJSON *properties = cJSON_CreateObject();
    cJSON *required = cJSON_CreateArray();

    if (!properties || !required) {
        cJSON_Delete(schema);
        return NULL;
    }

    // Process each parameter
    for (int i = 0; parameters[i].name != NULL; i++) {
        const openrouter_param_t *param = &parameters[i];

        cJSON *prop = cJSON_CreateObject();
        if (!prop) {
            continue;
        }

        cJSON_AddStringToObject(prop, "type", param->type);
        if (param->description) {
            cJSON_AddStringToObject(prop, "description", param->description);
        }

        // Handle enum values
        if (param->enum_values) {
            cJSON *enum_array = cJSON_CreateArray();
            if (enum_array) {
                for (int j = 0; param->enum_values[j] != NULL; j++) {
                    cJSON_AddItemToArray(enum_array, cJSON_CreateString(param->enum_values[j]));
                }
                cJSON_AddItemToObject(prop, "enum", enum_array);
            }
        }

        cJSON_AddItemToObject(properties, param->name, prop);

        // Add to required array if needed
        if (param->required) {
            cJSON_AddItemToArray(required, cJSON_CreateString(param->name));
        }
    }

    cJSON_AddItemToObject(schema, "properties", properties);
    if (cJSON_GetArraySize(required) > 0) {
        cJSON_AddItemToObject(schema, "required", required);
    } else {
        cJSON_Delete(required);
    }

    return schema;
}

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
esp_err_t openrouter_register_simple_function(openrouter_handle_t handle, const openrouter_simple_function_t *function)
{
    if (!handle || !function || !function->name || !function->callback) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create JSON schema from simplified parameters
    cJSON *schema = create_json_schema_from_params(function->parameters);

    // Create the full function definition
    openrouter_function_t full_function = {.name = function->name,
                                           .description = function->description,
                                           .parameters = schema,
                                           .callback = function->callback,
                                           .user_data = function->user_data};

    // Register using the existing function
    esp_err_t result = openrouter_register_function(handle, &full_function);

    // Clean up the schema (it's copied in register_function)
    if (schema) {
        cJSON_Delete(schema);
    }

    return result;
}

/**
 * @brief Unregister a function by name
 *
 * @param handle OpenRouter handle
 * @param function_name Name of the function to unregister
 * @return esp_err_t ESP_OK on success, ESP_ERR_NOT_FOUND if function not found
 */
esp_err_t openrouter_unregister_function(openrouter_handle_t handle, const char *function_name)
{
    if (!handle || !function_name) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    // Find function to remove
    int found_index = -1;
    for (int i = 0; i < handle->function_count; i++) {
        if (handle->functions[i].name && strcmp(handle->functions[i].name, function_name) == 0) {
            found_index = i;
            break;
        }
    }

    if (found_index == -1) {
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_NOT_FOUND;
    }

    // Free resources for found function
    registered_function_t *reg_func = &handle->functions[found_index];
    free(reg_func->name);
    free(reg_func->description);
    if (reg_func->parameters) {
        cJSON_Delete(reg_func->parameters);
    }

    // Shift remaining functions down
    for (int i = found_index; i < handle->function_count - 1; i++) {
        handle->functions[i] = handle->functions[i + 1];
    }

    // Clear the last slot
    memset(&handle->functions[handle->function_count - 1], 0, sizeof(registered_function_t));
    handle->function_count--;

    xSemaphoreGive(handle->mutex);

    ESP_LOGI(TAG, "Unregistered function: %s", function_name);
    return ESP_OK;
}

/**
 * @brief Clear all registered functions
 *
 * @param handle OpenRouter handle
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_clear_functions(openrouter_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    // Free all registered functions
    for (int i = 0; i < handle->function_count; i++) {
        if (handle->functions[i].name) {
            free(handle->functions[i].name);
        }
        if (handle->functions[i].description) {
            free(handle->functions[i].description);
        }
        if (handle->functions[i].parameters) {
            cJSON_Delete(handle->functions[i].parameters);
        }
    }

    // Clear the array
    memset(handle->functions, 0, sizeof(handle->functions));
    handle->function_count = 0;

    xSemaphoreGive(handle->mutex);

    ESP_LOGI(TAG, "Cleared all registered functions");
    return ESP_OK;
}

/**
 * @brief Enable or disable tools/function calling
 *
 * @param handle OpenRouter handle
 * @param enable_tools true to enable tools, false to disable
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_tools_enabled(openrouter_handle_t handle, bool enable_tools)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->enable_tools = enable_tools;
    xSemaphoreGive(handle->mutex);

    return ESP_OK;
}

/**
 * @brief Set the tool callback function
 *
 * @param handle OpenRouter handle
 * @param callback Tool callback function
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_set_tool_callback(openrouter_handle_t handle, openrouter_tool_callback_t callback, void *user_data)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->tool_callback = callback;
    handle->tool_user_data = user_data;
    xSemaphoreGive(handle->mutex);

    return ESP_OK;
}

/**
 * @brief Create a snapshot of the configuration for thread-safe access
 *
 * @param handle OpenRouter handle
 * @param snapshot Pointer to snapshot structure to fill
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
static esp_err_t create_config_snapshot(openrouter_handle_t handle, config_snapshot_t *snapshot)
{
    CHECK_NULL_AND_RETURN(handle, ESP_ERR_INVALID_ARG);
    CHECK_NULL_AND_RETURN(snapshot, ESP_ERR_INVALID_ARG);

    memset(snapshot, 0, sizeof(config_snapshot_t));

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    /* Copy string fields using safe_strdup */
    if (handle->api_key) {
        snapshot->api_key = safe_strdup(handle->api_key);
        if (!snapshot->api_key) {
            LOG_AND_GOTO_ERROR(cleanup, "Failed to copy API key");
        }
    }

    if (handle->model) {
        snapshot->model = safe_strdup(handle->model);
        if (!snapshot->model) {
            LOG_AND_GOTO_ERROR(cleanup, "Failed to copy model");
        }
    }

    if (handle->system_role) {
        snapshot->system_role = safe_strdup(handle->system_role);
        if (!snapshot->system_role) {
            LOG_AND_GOTO_ERROR(cleanup, "Failed to copy system role");
        }
    }

    if (handle->http_referer) {
        snapshot->http_referer = safe_strdup(handle->http_referer);
        if (!snapshot->http_referer) {
            LOG_AND_GOTO_ERROR(cleanup, "Failed to copy HTTP referer");
        }
    }

    if (handle->x_title) {
        snapshot->x_title = safe_strdup(handle->x_title);
        if (!snapshot->x_title) {
            LOG_AND_GOTO_ERROR(cleanup, "Failed to copy X-Title");
        }
    }

    /* Copy primitive fields */
    snapshot->temperature = handle->temperature;
    snapshot->max_tokens = handle->max_tokens;
    snapshot->top_p = handle->top_p;
    snapshot->seed = handle->seed;
    snapshot->enable_streaming = handle->enable_streaming;
    snapshot->http_timeout_ms = handle->http_timeout_ms;
    snapshot->enable_tools = handle->enable_tools;
    snapshot->tool_callback = handle->tool_callback;
    snapshot->tool_user_data = handle->tool_user_data;

    /* Copy retry configuration */
    snapshot->max_retry_attempts = handle->max_retry_attempts;
    snapshot->retry_delay_ms = handle->retry_delay_ms;
    snapshot->max_retry_delay_ms = handle->max_retry_delay_ms;

    /* Create tools array if tools are enabled and functions are registered */
    snapshot->tools_array = NULL;
    if (handle->enable_tools && handle->function_count > 0) {
        snapshot->tools_array = cJSON_CreateArray();
        if (snapshot->tools_array) {
            for (int i = 0; i < handle->function_count; i++) {
                cJSON *tool = cJSON_CreateObject();
                if (tool) {
                    cJSON_AddStringToObject(tool, "type", "function");
                    cJSON *function_obj = cJSON_CreateObject();
                    if (function_obj) {
                        cJSON_AddStringToObject(function_obj, "name", handle->functions[i].name);
                        if (handle->functions[i].description) {
                            cJSON_AddStringToObject(function_obj, "description", handle->functions[i].description);
                        }
                        if (handle->functions[i].parameters) {
                            cJSON_AddItemToObject(function_obj, "parameters",
                                                  cJSON_Duplicate(handle->functions[i].parameters, true));
                        }
                        cJSON_AddItemToObject(tool, "function", function_obj);
                        cJSON_AddItemToArray(snapshot->tools_array, tool);
                    } else {
                        cJSON_Delete(tool);
                    }
                } else {
                    ESP_LOGW(TAG, "Failed to create tool object for function %s", handle->functions[i].name);
                }
            }
        } else {
            ESP_LOGW(TAG, "Failed to create tools array");
        }
    }

    xSemaphoreGive(handle->mutex);
    return ESP_OK;

cleanup:
    /* Unified cleanup on error */
    if (snapshot->api_key) {
        free(snapshot->api_key);
        snapshot->api_key = NULL;
    }
    if (snapshot->model) {
        free(snapshot->model);
        snapshot->model = NULL;
    }
    if (snapshot->system_role) {
        free(snapshot->system_role);
        snapshot->system_role = NULL;
    }
    if (snapshot->http_referer) {
        free(snapshot->http_referer);
        snapshot->http_referer = NULL;
    }
    if (snapshot->x_title) {
        free(snapshot->x_title);
        snapshot->x_title = NULL;
    }
    if (snapshot->tools_array) {
        cJSON_Delete(snapshot->tools_array);
        snapshot->tools_array = NULL;
    }
    memset(snapshot, 0, sizeof(config_snapshot_t));
    xSemaphoreGive(handle->mutex);
    return ESP_ERR_NO_MEM;
}

/**
 * @brief Free resources used by a config snapshot
 *
 * @param snapshot Pointer to snapshot structure
 */
static void destroy_config_snapshot(config_snapshot_t *snapshot)
{
    if (!snapshot) {
        return;
    }

    if (snapshot->api_key) {
        free(snapshot->api_key);
        snapshot->api_key = NULL;
    }
    if (snapshot->model) {
        free(snapshot->model);
        snapshot->model = NULL;
    }
    if (snapshot->system_role) {
        free(snapshot->system_role);
        snapshot->system_role = NULL;
    }
    if (snapshot->http_referer) {
        free(snapshot->http_referer);
        snapshot->http_referer = NULL;
    }
    if (snapshot->x_title) {
        free(snapshot->x_title);
        snapshot->x_title = NULL;
    }
    if (snapshot->tools_array) {
        cJSON_Delete(snapshot->tools_array);
        snapshot->tools_array = NULL;
    }
    memset(snapshot, 0, sizeof(config_snapshot_t));
}

/**
 * @brief Create JSON payload for API request
 *
 * @param config Configuration snapshot
 * @param messages Messages array
 * @param enable_streaming Whether to enable streaming
 * @return char* JSON string (must be freed by caller), or NULL on failure
 */
static char *build_json_payload(const config_snapshot_t *config, cJSON *messages, bool enable_streaming)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    cJSON_AddStringToObject(root, "model", config->model);
    cJSON_AddItemReferenceToObject(root, "messages", messages);
    cJSON_AddNumberToObject(root, "temperature", config->temperature);
    cJSON_AddNumberToObject(root, "max_tokens", config->max_tokens);
    cJSON_AddNumberToObject(root, "top_p", config->top_p);
    cJSON_AddBoolToObject(root, "stream", enable_streaming);
    if (config->seed != -1) {
        cJSON_AddNumberToObject(root, "seed", config->seed);
    }

    // Add tools if enabled and available
    if (config->enable_tools && config->tools_array && cJSON_GetArraySize(config->tools_array) > 0) {
        cJSON_AddItemReferenceToObject(root, "tools", config->tools_array);
        // Only add tool_choice for models that support it - many models auto-detect
        // cJSON_AddStringToObject(root, "tool_choice", "auto");
    }

    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!post_data) {
        ESP_LOGE(TAG, "Failed to create JSON payload");
        return NULL;
    }

    return post_data;
}

/**
 * @brief Execute a function call using registered callbacks
 *
 * @param handle OpenRouter handle
 * @param function_name Name of function to call
 * @param arguments JSON string with function arguments
 * @return char* JSON result string (must be freed by caller), or NULL on error
 */
static char *execute_function_call(openrouter_handle_t handle, const char *function_name, const char *arguments)
{
    if (!handle || !function_name) {
        return NULL;
    }

    // Find the registered function
    registered_function_t *func = NULL;
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    for (int i = 0; i < handle->function_count; i++) {
        if (handle->functions[i].name && strcmp(handle->functions[i].name, function_name) == 0) {
            func = &handle->functions[i];
            break;
        }
    }
    xSemaphoreGive(handle->mutex);

    if (!func) {
        ESP_LOGE(TAG, "Function not found: %s", function_name);
        return NULL;
    }

    // Call the function
    char *result = func->callback(function_name, arguments ? arguments : "{}", func->user_data);
    return result;
}

/**
 * @brief Process tool calls in a response
 *
 * @param handle OpenRouter handle
 * @param response_json The JSON response containing tool calls
 * @param messages Messages array to append to
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
static esp_err_t process_tool_calls(openrouter_handle_t handle, cJSON *response_json, cJSON *messages)
{
    if (!handle || !response_json || !messages) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *choices = cJSON_GetObjectItem(response_json, "choices");
    if (!choices || !cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *choice = cJSON_GetArrayItem(choices, 0);
    if (!choice) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *message = cJSON_GetObjectItem(choice, "message");
    if (!message) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");
    if (!tool_calls || !cJSON_IsArray(tool_calls)) {
        return ESP_ERR_INVALID_ARG;
    }

    // Add the assistant's message with tool calls to the conversation
    cJSON *assistant_msg = cJSON_CreateObject();
    if (!assistant_msg) {
        return ESP_ERR_OPENROUTER_MEMORY;
    }

    cJSON_AddStringToObject(assistant_msg, "role", "assistant");
    cJSON *content = cJSON_GetObjectItem(message, "content");
    if (content && cJSON_IsString(content) && content->valuestring) {
        cJSON_AddStringToObject(assistant_msg, "content", content->valuestring);
    } else {
        cJSON_AddStringToObject(assistant_msg, "content", "");
    }
    cJSON_AddItemToObject(assistant_msg, "tool_calls", cJSON_Duplicate(tool_calls, true));
    cJSON_AddItemToArray(messages, assistant_msg);

    // Process each tool call
    int tool_call_count = cJSON_GetArraySize(tool_calls);
    for (int i = 0; i < tool_call_count; i++) {
        cJSON *tool_call = cJSON_GetArrayItem(tool_calls, i);
        if (!tool_call)
            continue;

        cJSON *id = cJSON_GetObjectItem(tool_call, "id");
        cJSON *function = cJSON_GetObjectItem(tool_call, "function");
        if (!id || !function)
            continue;

        cJSON *name = cJSON_GetObjectItem(function, "name");
        cJSON *arguments = cJSON_GetObjectItem(function, "arguments");
        if (!name || !cJSON_IsString(name))
            continue;

        const char *function_name = name->valuestring;
        const char *function_args = arguments && cJSON_IsString(arguments) ? arguments->valuestring : "{}";
        const char *tool_call_id = id->valuestring;
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
        ESP_LOGI(TAG, "Executing tool call: %s with args: %s", function_name, function_args);
#endif

        // Execute the function
        char *result = NULL;
        if (handle->tool_callback) {
            // Use tool callback if set
            result = handle->tool_callback(tool_call_id, function_name, function_args, handle->tool_user_data);
        } else {
            // Fall back to registered function callback
            result = execute_function_call(handle, function_name, function_args);
        }

        // Create tool response message
        cJSON *tool_msg = cJSON_CreateObject();
        if (tool_msg) {
            cJSON_AddStringToObject(tool_msg, "role", "tool");
            cJSON_AddStringToObject(tool_msg, "tool_call_id", tool_call_id);
            if (result) {
                cJSON_AddStringToObject(tool_msg, "content", result);
                free(result);
            } else {
                cJSON_AddStringToObject(tool_msg, "content", "{\"error\": \"Function execution failed\"}");
            }
            cJSON_AddItemToArray(messages, tool_msg);
        }
    }

    return ESP_OK;
}

/* ================================================================================
 * API CALL IMPLEMENTATION
 * ================================================================================ */

// Non-streaming API call implementation
static esp_err_t perform_call_non_streaming(openrouter_handle_t handle, cJSON *messages, char *response,
                                            size_t response_size)
{
    // Create thread-safe config snapshot
    config_snapshot_t config;
    esp_err_t ret = create_config_snapshot(handle, &config);
    if (ret != ESP_OK) {
        return ret;
    }

    char *post_data = build_json_payload(&config, messages, false);
    if (!post_data) {
        destroy_config_snapshot(&config);
        return ESP_ERR_OPENROUTER_MEMORY;
    }

    // Always log the request when tools are enabled for debugging
    if (config.enable_tools) {
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
        ESP_LOGI(TAG, "Sending request with tools: %s", post_data);
#endif
    }

#if CONFIG_OPENROUTER_VERBOSE_LOGGING
    ESP_LOGI(TAG, "Sending non-streaming request: %s", post_data);
#endif

    // Initialize response data structure
    response_data_t resp_data = {
        .buffer = calloc(1, INITIAL_RESPONSE_BUFFER_SIZE), .size = INITIAL_RESPONSE_BUFFER_SIZE, .pos = 0};

    if (!resp_data.buffer) {
        free(post_data);
        return ESP_ERR_NO_MEM;
    }
    resp_data.buffer[0] = '\0';

    esp_http_client_config_t http_config = {
        .url = API_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler_non_streaming,
        .user_data = &resp_data,
        .timeout_ms = config.http_timeout_ms,
        .skip_cert_common_name_check = false,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
        .crt_bundle_attach = esp_crt_bundle_attach,
#else
        .use_global_ca_store = true,
#endif
    };

    esp_http_client_handle_t client = NULL;
    bool is_reused = false;
    esp_err_t get_client_err = get_http_client(handle, &http_config, &client, &is_reused);
    if (get_client_err != ESP_OK || !client) {
        ESP_LOGE(TAG, "Failed to get HTTP client: %s", esp_err_to_name(get_client_err));
        free(post_data);
        free(resp_data.buffer);
        destroy_config_snapshot(&config);
        return ESP_ERR_OPENROUTER_HTTP_ERROR;
    }

    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", config.api_key);
    esp_http_client_set_header(client, "Authorization", auth_header);

    // Set optional headers
    if (config.http_referer) {
        esp_http_client_set_header(client, "Referer", config.http_referer);
    }
    if (config.x_title) {
        esp_http_client_set_header(client, "X-Title", config.x_title);
    }

    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // Implement retry logic for transient network errors
    esp_err_t err = ESP_FAIL;
    int retry_count = 0;
    bool should_retry = false;

    do {
        if (retry_count > 0) {
            // Calculate delay with exponential backoff and jitter
            uint32_t delay_ms =
                calculate_retry_delay(retry_count - 1, config.retry_delay_ms, config.max_retry_delay_ms);
            ESP_LOGW(TAG, "Retrying API request (attempt %d of %d) after %" PRIu32 " ms", retry_count,
                     config.max_retry_attempts, delay_ms);
            // Reset buffer for retry
            resp_data.pos = 0;
            resp_data.buffer[0] = '\0';
            // Wait before retry
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }

        err = esp_http_client_perform(client);
        should_retry = false;

        // Determine if we should retry based on error type
        if (err != ESP_OK) {
            // Retry on network errors (timeout, connection failed, etc.)
            should_retry = (err == ESP_ERR_HTTP_CONNECT || err == ESP_ERR_HTTP_CONNECTING ||
                            err == ESP_ERR_HTTP_CONNECTION_CLOSED || err == ESP_ERR_HTTP_FETCH_HEADER ||
                            err == ESP_ERR_HTTP_TIMEOUT);

            ESP_LOGW(TAG, "HTTP request failed with error %s (%d), %s", esp_err_to_name(err), err,
                     should_retry ? "will retry" : "will not retry");
        } else {
            int status = esp_http_client_get_status_code(client);

            // Retry on specific HTTP status codes that indicate temporary issues
            should_retry = (status == 429 || status == 502 || status == 503 || status == 504);

            if (should_retry) {
                ESP_LOGW(TAG, "HTTP request returned status %d, will retry", status);

                // Check for Retry-After header on 429 (rate limit) responses
                if (status == 429) {
                    uint32_t retry_after_ms = parse_retry_after_header(client);
                    if (retry_after_ms > 0) {
                        ESP_LOGW(TAG, "Respecting Retry-After header: %" PRIu32 " ms", retry_after_ms);
                        vTaskDelay(pdMS_TO_TICKS(retry_after_ms));
                        // Skip the normal delay calculation for this iteration
                        retry_count++;
                        continue;
                    }
                }
            }
        }

        retry_count++;
    } while (should_retry && retry_count <= config.max_retry_attempts);

    // Process the final response (success or failure)
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
        ESP_LOGI(TAG, "HTTP Status: %d", status);
#endif

        if (status == 200) {
            if (resp_data.buffer && resp_data.pos > 0) {
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
                ESP_LOGI(TAG, "Raw Response: %s", resp_data.buffer);
#endif

                cJSON *json = cJSON_Parse(resp_data.buffer);
                if (json) {
                    // Check for error in response
                    cJSON *error = cJSON_GetObjectItem(json, "error");
                    if (error) {
                        cJSON *error_msg = cJSON_GetObjectItem(error, "message");
                        const char *error_text =
                            error_msg && cJSON_IsString(error_msg) ? error_msg->valuestring : "Unknown API error";
                        ESP_LOGE(TAG, "API Error: %s", error_text);

                        // Copy error message to response buffer for user visibility
                        snprintf(response, response_size, "Error: %s", error_text);
                        err = ESP_ERR_OPENROUTER_API_ERROR;
                    } else {
                        // Parse successful response
                        cJSON *choices = cJSON_GetObjectItem(json, "choices");
                        if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
                            cJSON *choice = cJSON_GetArrayItem(choices, 0);
                            if (choice) {
                                cJSON *message = cJSON_GetObjectItem(choice, "message");
                                if (message) {
                                    // Check for refusal first
                                    cJSON *refusal = cJSON_GetObjectItem(message, "refusal");
                                    if (refusal && cJSON_IsString(refusal) && refusal->valuestring) {
                                        snprintf(response, response_size, "[REFUSAL] %s", refusal->valuestring);
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
                                        ESP_LOGI(TAG, "Extracted refusal: %s", response);
#endif
                                    }
                                    // Check for tool calls
                                    else if (cJSON_HasObjectItem(message, "tool_calls")) {
                                        cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");
                                        char *tool_calls_str = cJSON_Print(tool_calls);
                                        if (tool_calls_str) {
                                            snprintf(response, response_size, "[TOOL_CALLS] %s", tool_calls_str);
                                            free(tool_calls_str);
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
                                            ESP_LOGI(TAG, "Extracted tool calls: %s", response);
#endif
                                        }
                                    }
                                    // Check for function call (legacy)
                                    else if (cJSON_HasObjectItem(message, "function_call")) {
                                        cJSON *function_call = cJSON_GetObjectItem(message, "function_call");
                                        char *function_call_str = cJSON_Print(function_call);
                                        if (function_call_str) {
                                            snprintf(response, response_size, "[FUNCTION_CALL] %s", function_call_str);
                                            free(function_call_str);
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
                                            ESP_LOGI(TAG, "Extracted function call: %s", response);
#endif
                                        }
                                    }
                                    // Standard content response
                                    else {
                                        cJSON *content = cJSON_GetObjectItem(message, "content");
                                        if (content && cJSON_IsString(content) && content->valuestring) {
                                            snprintf(response, response_size, "%s", content->valuestring);
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
                                            ESP_LOGI(TAG, "Extracted response: %s", response);
#endif
                                        } else {
                                            ESP_LOGE(TAG, "No content found in message or content is not a string");
                                            snprintf(response, response_size, "Error: No content in response");
                                            err = ESP_ERR_OPENROUTER_JSON_PARSE;
                                        }
                                    }
                                } else {
                                    ESP_LOGE(TAG, "No message found in choice");
                                    snprintf(response, response_size, "Error: Malformed response (no message)");
                                    err = ESP_ERR_OPENROUTER_JSON_PARSE;
                                }
                            } else {
                                ESP_LOGE(TAG, "No valid choice found");
                                snprintf(response, response_size, "Error: Malformed response (no choice)");
                                err = ESP_ERR_OPENROUTER_JSON_PARSE;
                            }
                        } else {
                            ESP_LOGE(TAG, "No choices array found or empty");
                            snprintf(response, response_size, "Error: Malformed response (no choices)");
                            err = ESP_ERR_OPENROUTER_JSON_PARSE;
                        }
                    }
                    cJSON_Delete(json);
                } else {
                    ESP_LOGE(TAG, "Failed to parse JSON response");
                    err = ESP_FAIL;
                }
            } else {
                ESP_LOGE(TAG, "No response data received");
                err = ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "HTTP request failed with status: %d", status);
            if (resp_data.buffer && resp_data.pos > 0) {
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
                ESP_LOGE(TAG, "Error response: %s", resp_data.buffer);
#endif

                // Try to parse error response JSON
                cJSON *json = cJSON_Parse(resp_data.buffer);
                if (json) {
                    cJSON *error = cJSON_GetObjectItem(json, "error");
                    if (error) {
                        cJSON *error_msg = cJSON_GetObjectItem(error, "message");
                        if (error_msg && cJSON_IsString(error_msg)) {
                            ESP_LOGE(TAG, "API Error: %s", error_msg->valuestring);
                            // Copy error message to response buffer for user visibility
                            snprintf(response, response_size, "Error: %s", error_msg->valuestring);
                        } else {
                            snprintf(response, response_size, "Error: HTTP %d - Unknown API error", status);
                        }
                    } else {
                        snprintf(response, response_size, "Error: HTTP %d - %.*s", status, (int) (response_size - 50),
                                 resp_data.buffer);
                    }
                    cJSON_Delete(json);
                } else {
                    // Raw error response
                    snprintf(response, response_size, "Error: HTTP %d - %.*s", status, (int) (response_size - 50),
                             resp_data.buffer);
                }
            }
            err = ESP_ERR_OPENROUTER_API_ERROR;
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    free(post_data);
    free(resp_data.buffer);
    release_http_client(handle, client);
    destroy_config_snapshot(&config);
    return err;
}

// Streaming API call implementation
static esp_err_t perform_call_streaming(openrouter_handle_t handle, cJSON *messages,
                                        openrouter_stream_callback_t callback, void *user_data)
{
    // Create thread-safe config snapshot
    config_snapshot_t config;
    esp_err_t ret = create_config_snapshot(handle, &config);
    if (ret != ESP_OK) {
        return ret;
    }

    char *post_data = build_json_payload(&config, messages, true);
    if (!post_data) {
        destroy_config_snapshot(&config);
        return ESP_ERR_OPENROUTER_MEMORY;
    }
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
    ESP_LOGI(TAG, "Sending streaming request: %s", post_data);
#endif

    // Initialize streaming response data structure
    streaming_response_data_t resp_data = {
        .buffer = calloc(1, INITIAL_RESPONSE_BUFFER_SIZE), // Allocate buffer for accumulating full response
        .size = INITIAL_RESPONSE_BUFFER_SIZE,
        .pos = 0,
        .callback = callback,
        .user_data = user_data,
        .line_buffer = calloc(1, INITIAL_LINE_BUFFER_SIZE),
        .line_buffer_size = INITIAL_LINE_BUFFER_SIZE,
        .line_buffer_pos = 0,
        .done_sent = false,
        .accumulated_data = calloc(1, INITIAL_RESPONSE_BUFFER_SIZE),
        .accumulated_data_size = INITIAL_RESPONSE_BUFFER_SIZE,
        .accumulated_data_pos = 0};

    if (!resp_data.line_buffer || !resp_data.buffer || !resp_data.accumulated_data) {
        ESP_LOGE(TAG, "Failed to allocate response buffers");
        free(post_data);
        destroy_config_snapshot(&config);
        if (resp_data.line_buffer)
            free(resp_data.line_buffer);
        if (resp_data.buffer)
            free(resp_data.buffer);
        if (resp_data.accumulated_data)
            free(resp_data.accumulated_data);
        return ESP_ERR_NO_MEM;
    }
    resp_data.line_buffer[0] = '\0';
    resp_data.buffer[0] = '\0';
    resp_data.accumulated_data[0] = '\0';

    esp_http_client_config_t http_config = {
        .url = API_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler_streaming,
        .user_data = &resp_data,
        .timeout_ms = config.http_timeout_ms,
        .skip_cert_common_name_check = false,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
        .crt_bundle_attach = esp_crt_bundle_attach,
#else
        .use_global_ca_store = true,
#endif
    };

    esp_http_client_handle_t client = NULL;
    bool is_reused = false;
    esp_err_t get_client_err = get_http_client(handle, &http_config, &client, &is_reused);
    if (get_client_err != ESP_OK || !client) {
        ESP_LOGE(TAG, "Failed to get HTTP client for streaming: %s", esp_err_to_name(get_client_err));
        free(post_data);
        destroy_config_snapshot(&config);
        free(resp_data.line_buffer);
        free(resp_data.buffer);
        return ESP_ERR_OPENROUTER_HTTP_ERROR;
    }

    // Set headers for streaming
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "text/event-stream");
    esp_http_client_set_header(client, "Cache-Control", "no-cache");

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", config.api_key);
    esp_http_client_set_header(client, "Authorization", auth_header);

    // Set optional headers
    if (config.http_referer) {
        esp_http_client_set_header(client, "Referer", config.http_referer);
    }
    if (config.x_title) {
        esp_http_client_set_header(client, "X-Title", config.x_title);
    }

    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // Implement retry logic for transient network errors
    esp_err_t err = ESP_FAIL;
    int retry_count = 0;
    bool should_retry = false;

    do {
        if (retry_count > 0) {
            // Calculate delay with exponential backoff and jitter
            uint32_t delay_ms =
                calculate_retry_delay(retry_count - 1, config.retry_delay_ms, config.max_retry_delay_ms);
            ESP_LOGW(TAG, "Retrying streaming API request (attempt %d of %d) after %" PRIu32 " ms", retry_count,
                     config.max_retry_attempts, delay_ms);

            // Reset buffers for retry
            resp_data.pos = 0;
            resp_data.buffer[0] = '\0';
            resp_data.line_buffer_pos = 0;
            resp_data.line_buffer[0] = '\0';
            resp_data.accumulated_data_pos = 0;
            resp_data.accumulated_data[0] = '\0';
            resp_data.done_sent = false;

            // Notify client about retry
            if (callback) {
                callback("[Retrying connection...]", false, user_data);
            }

            // Wait before retry
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }

        err = esp_http_client_perform(client);
        should_retry = false;

        // Determine if we should retry based on error type
        if (err != ESP_OK) {
            // Retry on network errors (timeout, connection failed, etc.)
            should_retry = (err == ESP_ERR_HTTP_CONNECT || err == ESP_ERR_HTTP_CONNECTING ||
                            err == ESP_ERR_HTTP_CONNECTION_CLOSED || err == ESP_ERR_HTTP_FETCH_HEADER ||
                            err == ESP_ERR_HTTP_TIMEOUT);

            ESP_LOGW(TAG, "HTTP streaming request failed with error %s (%d), %s", esp_err_to_name(err), err,
                     should_retry ? "will retry" : "will not retry");

            // Notify client about error
            if (callback && !should_retry) {
                char error_msg[128];
                snprintf(error_msg, sizeof(error_msg), "[ERROR: Connection failed: %s]", esp_err_to_name(err));
                callback(error_msg, true, user_data);
            }
        } else {
            int status = esp_http_client_get_status_code(client);

            // Retry on specific HTTP status codes that indicate temporary issues
            should_retry = (status == 429 || status == 502 || status == 503 || status == 504);

            if (should_retry) {
                ESP_LOGW(TAG, "HTTP streaming request returned status %d, will retry", status);

                // Check for Retry-After header on 429 (rate limit) responses
                if (status == 429) {
                    uint32_t retry_after_ms = parse_retry_after_header(client);
                    if (retry_after_ms > 0) {
                        ESP_LOGW(TAG, "Respecting Retry-After header: %" PRIu32 " ms", retry_after_ms);
                        vTaskDelay(pdMS_TO_TICKS(retry_after_ms));
                        // Skip the normal delay calculation for this iteration
                        retry_count++;
                        continue;
                    }
                }
            } else if (status != 200) {
                ESP_LOGE(TAG, "HTTP streaming request failed with status: %d", status);

                // Notify client about error
                if (callback) {
                    char error_msg[128];
                    snprintf(error_msg, sizeof(error_msg), "[ERROR: Server returned status %d]", status);
                    callback(error_msg, true, user_data);
                }

                err = ESP_ERR_OPENROUTER_API_ERROR;
            }
        }

        retry_count++;
    } while (should_retry && retry_count <= config.max_retry_attempts);

    free(post_data);
    free(resp_data.line_buffer);
    free(resp_data.buffer);
    free(resp_data.accumulated_data);
    release_http_client(handle, client);
    destroy_config_snapshot(&config);
    return err;
}


/**
 * @brief Encode binary data to base64
 *
 * @param data Input binary data
 * @param input_length Length of input data
 * @param output_length Pointer to store output length
 * @return char* Base64 encoded string (must be freed by caller), NULL on error
 */

static char *base64_encode(const unsigned char *data, size_t input_length, size_t *output_length)
{
    if (!data || input_length == 0) {
        return NULL;
    }

    *output_length = BASE64_ENCODE_OUT_SIZE(input_length);
    char *encoded = malloc(*output_length);
    if (!encoded) {
        ESP_LOGE(TAG, "Failed to allocate base64 buffer");
        return NULL;
    }

    size_t i, j;
    for (i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded[j++] = base64_chars[(triple >> 3 * 6) & 0x3F];
        encoded[j++] = base64_chars[(triple >> 2 * 6) & 0x3F];
        encoded[j++] = base64_chars[(triple >> 1 * 6) & 0x3F];
        encoded[j++] = base64_chars[(triple >> 0 * 6) & 0x3F];
    }

    static const int mod_table[] = {0, 2, 1};
    for (int i = 0; i < mod_table[input_length % 3]; i++) {
        encoded[*output_length - 1 - i] = '=';
    }

    encoded[*output_length - 1] = '\0';
    (*output_length)--; // Don't count null terminator

    return encoded;
}

/**
 * @brief Get MIME type from file extension
 *
 * @param filename Filename to analyze
 * @param is_audio true to check audio types, false for image types
 * @return const char* MIME type string or NULL if not supported
 */
static const char *get_mime_type_from_extension(const char *filename, bool is_audio)
{
    if (!filename) {
        return NULL;
    }

    // Find the last dot in filename
    const char *ext = strrchr(filename, '.');
    if (!ext) {
        return NULL;
    }

    // Convert to lowercase for comparison
    char ext_lower[16];
    size_t ext_len = strlen(ext);
    if (ext_len >= sizeof(ext_lower)) {
        return NULL;
    }

    for (size_t i = 0; i <= ext_len; i++) {
        ext_lower[i] = tolower(ext[i]);
    }

    if (is_audio) {
        for (int i = 0; audio_types[i].extension != NULL; i++) {
            if (strcmp(ext_lower, audio_types[i].extension) == 0) {
                return audio_types[i].mime_type;
            }
        }
    } else {
        for (int i = 0; image_types[i].extension != NULL; i++) {
            if (strcmp(ext_lower, image_types[i].extension) == 0) {
                return image_types[i].mime_type;
            }
        }
    }

    return NULL;
}

/**
 * @brief Detect MIME type from file header
 *
 * @param data File data
 * @param data_size Size of data
 * @param is_audio true to check audio types, false for image types
 * @return const char* MIME type string or NULL if not detected
 */
static const char *detect_mime_type_from_header(const unsigned char *data, size_t data_size, bool is_audio)
{
    if (!data || data_size < 4) {
        return NULL;
    }

    if (!is_audio) {
        // Check for image signatures
        if (data_size >= 8 && data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
            return "image/png"; // PNG signature
        }
        if (data_size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
            return "image/jpeg"; // JPEG signature
        }
    } else {
        // Check for audio signatures
        if (data_size >= 3 && data[0] == 'I' && data[1] == 'D' && data[2] == '3') {
            return "audio/mpeg"; // MP3 with ID3 tag
        }
        if (data_size >= 4 && data[0] == 0xFF && (data[1] & 0xF0) == 0xF0) {
            return "audio/mpeg"; // MP3 frame header
        }
        if (data_size >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WAVE", 4) == 0) {
            return "audio/wav"; // WAV signature
        }
    }

    return NULL;
}

/**
 * @brief Read file from filesystem
 *
 * @param filepath Path to file
 * @param data_out Pointer to store allocated data (must be freed by caller)
 * @param size_out Pointer to store file size
 * @param max_size Maximum allowed file size
 * @return esp_err_t ESP_OK on success, error code on failure
 */
static esp_err_t read_file_data(const char *filepath, unsigned char **data_out, size_t *size_out, size_t max_size)
{
    if (!filepath || !data_out || !size_out) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        ESP_LOGE(TAG, "Invalid file size: %ld", file_size);
        fclose(file);
        return ESP_ERR_INVALID_SIZE;
    }

    if ((size_t) file_size > max_size) {
        ESP_LOGE(TAG, "File too large: %ld bytes (max: %zu)", file_size, max_size);
        fclose(file);
        return ESP_ERR_INVALID_SIZE;
    }

    // Allocate buffer
    unsigned char *data = malloc(file_size);
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate file buffer (%ld bytes)", file_size);
        fclose(file);
        return ESP_ERR_NO_MEM;
    }

    // Read file
    size_t bytes_read = fread(data, 1, file_size, file);
    fclose(file);

    if (bytes_read != (size_t) file_size) {
        ESP_LOGE(TAG, "Failed to read complete file: %zu/%ld bytes", bytes_read, file_size);
        free(data);
        return ESP_ERR_INVALID_RESPONSE;
    }

    *data_out = data;
    *size_out = file_size;
    return ESP_OK;
}

/**
 * @brief Create data URL from file data
 *
 * @param data File data
 * @param data_size Size of data
 * @param mime_type MIME type of the file
 * @return char* Data URL string (must be freed by caller), NULL on error
 */
static char *create_data_url(const unsigned char *data, size_t data_size, const char *mime_type)
{
    if (!data || data_size == 0 || !mime_type) {
        return NULL;
    }

    // Encode to base64
    size_t encoded_size;
    char *encoded = base64_encode(data, data_size, &encoded_size);
    if (!encoded) {
        return NULL;
    }

    // Calculate total size needed for data URL
    size_t prefix_len = strlen("data:") + strlen(mime_type) + strlen(";base64,");
    size_t total_size = prefix_len + encoded_size + 1;

    char *data_url = malloc(total_size);
    if (!data_url) {
        ESP_LOGE(TAG, "Failed to allocate data URL buffer");
        free(encoded);
        return NULL;
    }

    // Create data URL: data:mime/type;base64,encoded_data
    snprintf(data_url, total_size, "data:%s;base64,%s", mime_type, encoded);

    free(encoded);
    return data_url;
}

/**
 * @brief Add media content to message
 *
 * @param message_content cJSON array to add content to
 * @param source Media source (file path, URL, or data)
 * @param is_audio true for audio, false for image
 * @param is_file_path true if source is file path, false if URL or data
 * @param is_data_array true if source points to data array
 * @param data_size Size of data (only used if is_data_array is true)
 * @return esp_err_t ESP_OK on success, error code on failure
 */
static esp_err_t add_media_content(cJSON *message_content, const char *source, bool is_audio, bool is_file_path,
                                   bool is_data_array, size_t data_size)
{
    if (!message_content || !source) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *media_obj = cJSON_CreateObject();
    if (!media_obj) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    char *data_url = NULL;

    if (is_data_array) {
        // Source is actually a pointer to data array
        const unsigned char *data = (const unsigned char *) source;

        // Detect MIME type from data
        const char *mime_type = detect_mime_type_from_header(data, data_size, is_audio);
        if (!mime_type) {
            ESP_LOGE(TAG, "Could not detect MIME type from data");
            ret = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }

        // Create data URL
        data_url = create_data_url(data, data_size, mime_type);
        if (!data_url) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }

        cJSON_AddStringToObject(media_obj, is_audio ? "audio_url" : "image_url", data_url);

    } else if (is_file_path) {
        // Read file and convert to data URL
        unsigned char *file_data;
        size_t file_size;
        size_t max_size = is_audio ? MAX_AUDIO_FILE_SIZE : MAX_IMAGE_FILE_SIZE;

        ret = read_file_data(source, &file_data, &file_size, max_size);
        if (ret != ESP_OK) {
            goto cleanup;
        }

        // Try to get MIME type from extension first
        const char *mime_type = get_mime_type_from_extension(source, is_audio);
        if (!mime_type) {
            // Fall back to header detection
            mime_type = detect_mime_type_from_header(file_data, file_size, is_audio);
        }

        if (!mime_type) {
            ESP_LOGE(TAG, "Unsupported file type: %s", source);
            free(file_data);
            ret = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }

        // Create data URL
        data_url = create_data_url(file_data, file_size, mime_type);
        free(file_data);

        if (!data_url) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }

        cJSON_AddStringToObject(media_obj, is_audio ? "audio_url" : "image_url", data_url);

    } else {
        // Source is a URL
        cJSON_AddStringToObject(media_obj, is_audio ? "audio_url" : "image_url", source);
    }

    cJSON_AddStringToObject(media_obj, "type", is_audio ? "audio_url" : "image_url");
    cJSON_AddItemToArray(message_content, media_obj);

cleanup:
    if (data_url) {
        free(data_url);
    }
    if (ret != ESP_OK && media_obj) {
        cJSON_Delete(media_obj);
    }
    return ret;
}

/**
 * @brief Create message with text and media content
 *
 * @param text Text content
 * @param image_source Image source (path, URL, or data pointer)
 * @param audio_source Audio source (path, URL, or data pointer)
 * @param image_is_file true if image_source is file path
 * @param audio_is_file true if audio_source is file path
 * @param image_is_data true if image_source is data array
 * @param audio_is_data true if audio_source is data array
 * @param image_data_size Size of image data (if image_is_data)
 * @param audio_data_size Size of audio data (if audio_is_data)
 * @return cJSON* Message object or NULL on error
 */
static cJSON *create_multimodal_message(const char *text, const char *image_source, const char *audio_source,
                                        bool image_is_file, bool audio_is_file, bool image_is_data, bool audio_is_data,
                                        size_t image_data_size, size_t audio_data_size)
{
    cJSON *message = cJSON_CreateObject();
    if (!message) {
        return NULL;
    }

    cJSON_AddStringToObject(message, "role", "user");

    cJSON *content = cJSON_CreateArray();
    if (!content) {
        cJSON_Delete(message);
        return NULL;
    }

    // Add text content if provided
    if (text && strlen(text) > 0) {
        cJSON *text_obj = cJSON_CreateObject();
        if (!text_obj) {
            cJSON_Delete(content);
            cJSON_Delete(message);
            return NULL;
        }
        cJSON_AddStringToObject(text_obj, "type", "text");
        cJSON_AddStringToObject(text_obj, "text", text);
        cJSON_AddItemToArray(content, text_obj);
    }

    // Add image content if provided
    if (image_source) {
        esp_err_t ret = add_media_content(content, image_source, false, image_is_file, image_is_data, image_data_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add image content");
            cJSON_Delete(content);
            cJSON_Delete(message);
            return NULL;
        }
    }

    // Add audio content if provided
    if (audio_source) {
        esp_err_t ret = add_media_content(content, audio_source, true, audio_is_file, audio_is_data, audio_data_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add audio content");
            cJSON_Delete(content);
            cJSON_Delete(message);
            return NULL;
        }
    }

    cJSON_AddItemToObject(message, "content", content);
    return message;
}


/* ================================================================================
 * PUBLIC API FUNCTIONS
 * ================================================================================ */

/**
 * @brief Make a simple API call with just a prompt
 *
 * @param handle OpenRouter handle
 * @param prompt User prompt text
 * @param response Buffer to store response
 * @param response_size Size of response buffer
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call(openrouter_handle_t handle, const char *prompt, char *response, size_t response_size)
{
    if (!handle || !prompt || !response || response_size == 0) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    // Check streaming mode with proper locking
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    bool is_streaming = handle->enable_streaming;
    xSemaphoreGive(handle->mutex);

    if (is_streaming) {
        ESP_LOGE(TAG, "Handle configured for streaming - use openrouter_call_streaming instead");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        ESP_LOGE(TAG, "Failed to create messages array");
        return ESP_ERR_OPENROUTER_MEMORY;
    }

    esp_err_t err = ESP_OK;
    bool sys_msg_added = false;
    bool user_msg_added = false;

    // Add system message if configured (with thread-safe access)
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    char *system_role_copy = handle->system_role ? strdup(handle->system_role) : NULL;
    xSemaphoreGive(handle->mutex);

    if (system_role_copy) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (sys_msg) {
            cJSON_AddStringToObject(sys_msg, "role", "system");
            cJSON_AddStringToObject(sys_msg, "content", system_role_copy);
            cJSON_AddItemToArray(messages, sys_msg);
            sys_msg_added = true;
        } else {
            ESP_LOGW(TAG, "Failed to create system message object");
        }
        free(system_role_copy);
    }

    // Add user message
    cJSON *user_msg = cJSON_CreateObject();
    if (user_msg) {
        cJSON_AddStringToObject(user_msg, "role", "user");
        cJSON_AddStringToObject(user_msg, "content", prompt);
        cJSON_AddItemToArray(messages, user_msg);
        user_msg_added = true;
    } else {
        ESP_LOGE(TAG, "Failed to create user message object");
        err = ESP_ERR_OPENROUTER_MEMORY;
    }

    // Make the API call if messages were added successfully
    if (user_msg_added) {
        // Validate we have at least one message (either system or user)
        if (!sys_msg_added && !user_msg_added) {
            ESP_LOGE(TAG, "Failed to add any messages");
            err = ESP_ERR_OPENROUTER_MEMORY;
        } else {
            err = perform_call_non_streaming(handle, messages, response, response_size);
        }
    } else {
        err = ESP_ERR_OPENROUTER_MEMORY;
    }

    cJSON_Delete(messages);
    return err;
}

/**
 * @brief Make a simple streaming API call with just a prompt
 *
 * @param handle OpenRouter handle
 * @param prompt User prompt text
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_streaming(openrouter_handle_t handle, const char *prompt,
                                    openrouter_stream_callback_t callback, void *user_data)
{
    if (!handle || !prompt || !callback) {
        ESP_LOGE(TAG, "Invalid arguments for streaming call");
        return ESP_ERR_INVALID_ARG;
    }

    // Check streaming mode with proper locking
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    bool is_streaming = handle->enable_streaming;
    xSemaphoreGive(handle->mutex);

    if (!is_streaming) {
        ESP_LOGE(TAG, "Handle not configured for streaming - use openrouter_call instead or enable streaming");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        ESP_LOGE(TAG, "Failed to create messages array");
        return ESP_ERR_OPENROUTER_MEMORY;
    }

    esp_err_t err = ESP_OK;
    bool sys_msg_added = false;
    bool user_msg_added = false;

    // Add system message if configured (with thread-safe access)
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    char *system_role_copy = handle->system_role ? strdup(handle->system_role) : NULL;
    xSemaphoreGive(handle->mutex);

    if (system_role_copy) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (sys_msg) {
            cJSON_AddStringToObject(sys_msg, "role", "system");
            cJSON_AddStringToObject(sys_msg, "content", system_role_copy);
            cJSON_AddItemToArray(messages, sys_msg);
            sys_msg_added = true;
        } else {
            ESP_LOGW(TAG, "Failed to create system message object");
        }
        free(system_role_copy);
    }

    // Add user message
    cJSON *user_msg = cJSON_CreateObject();
    if (user_msg) {
        cJSON_AddStringToObject(user_msg, "role", "user");
        cJSON_AddStringToObject(user_msg, "content", prompt);
        cJSON_AddItemToArray(messages, user_msg);
        user_msg_added = true;
    } else {
        ESP_LOGE(TAG, "Failed to create user message object");
        err = ESP_ERR_OPENROUTER_MEMORY;
    }

    // Make the API call if messages were added successfully
    if (user_msg_added) {
        // Validate we have at least one message (either system or user)
        if (!sys_msg_added && !user_msg_added) {
            ESP_LOGE(TAG, "Failed to add any messages");
            err = ESP_ERR_OPENROUTER_MEMORY;
        } else {
            err = perform_call_streaming(handle, messages, callback, user_data);
        }
    } else {
        err = ESP_ERR_OPENROUTER_MEMORY;
    }

    cJSON_Delete(messages);
    return err;
}

/**
 * @brief Make an advanced API call with custom messages array
 *
 * @param handle OpenRouter handle
 * @param messages JSON array of message objects
 * @param response Buffer to store response
 * @param response_size Size of response buffer
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_ex(openrouter_handle_t handle, cJSON *messages, char *response, size_t response_size)
{
    if (!handle || !messages || !response || response_size == 0) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    // Check streaming mode with proper locking
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    bool is_streaming = handle->enable_streaming;
    xSemaphoreGive(handle->mutex);

    if (is_streaming) {
        ESP_LOGE(TAG, "Handle configured for streaming - use openrouter_call_ex_streaming instead");
        return ESP_ERR_INVALID_STATE;
    }

    if (!cJSON_IsArray(messages)) {
        ESP_LOGE(TAG, "Messages parameter must be a JSON array");
        return ESP_ERR_INVALID_ARG;
    }

    // Prepend system role if set and not already present
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    char *system_role_copy = handle->system_role ? strdup(handle->system_role) : NULL;
    xSemaphoreGive(handle->mutex);

    if (system_role_copy) {
        cJSON *first = cJSON_GetArrayItem(messages, 0);
        bool add_system = true;

        // Check if the first message is already a system message
        if (first) {
            cJSON *role = cJSON_GetObjectItem(first, "role");
            if (role && cJSON_IsString(role) && role->valuestring) {
                if (strcmp(role->valuestring, "system") == 0) {
                    add_system = false;
                }
            }
        }

        if (add_system) {
            cJSON *sys_msg = cJSON_CreateObject();
            if (sys_msg) {
                cJSON_AddStringToObject(sys_msg, "role", "system");
                cJSON_AddStringToObject(sys_msg, "content", system_role_copy);

                // Insert at the beginning of the array
                if (cJSON_GetArraySize(messages) > 0) {
                    cJSON_InsertItemInArray(messages, 0, sys_msg);
                } else {
                    cJSON_AddItemToArray(messages, sys_msg);
                }
            } else {
                ESP_LOGW(TAG, "Failed to create system message for prepending");
            }
        }
        free(system_role_copy);
    }

    esp_err_t result = perform_call_non_streaming(handle, messages, response, response_size);
    return result;
}

/**
 * @brief Make an advanced streaming API call with custom messages array
 *
 * @param handle OpenRouter handle
 * @param messages JSON array of message objects
 * @param callback Function to call for each chunk of streaming data
 * @param user_data User data to pass to callback
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t openrouter_call_ex_streaming(openrouter_handle_t handle, cJSON *messages,
                                       openrouter_stream_callback_t callback, void *user_data)
{
    if (!handle || !messages || !callback) {
        ESP_LOGE(TAG, "Invalid arguments for streaming call_ex");
        return ESP_ERR_INVALID_ARG;
    }

    // Check streaming mode with proper locking
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    bool is_streaming = handle->enable_streaming;
    xSemaphoreGive(handle->mutex);

    if (!is_streaming) {
        ESP_LOGE(TAG, "Handle not configured for streaming - use openrouter_call_ex instead or enable streaming");
        return ESP_ERR_INVALID_STATE;
    }

    if (!cJSON_IsArray(messages)) {
        ESP_LOGE(TAG, "Messages parameter must be a JSON array");
        return ESP_ERR_INVALID_ARG;
    }

    // Prepend system role if set and not already present
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    char *system_role_copy = handle->system_role ? strdup(handle->system_role) : NULL;
    xSemaphoreGive(handle->mutex);

    if (system_role_copy) {
        cJSON *first = cJSON_GetArrayItem(messages, 0);
        bool add_system = true;

        // Check if the first message is already a system message
        if (first) {
            cJSON *role = cJSON_GetObjectItem(first, "role");
            if (role && cJSON_IsString(role) && role->valuestring) {
                if (strcmp(role->valuestring, "system") == 0) {
                    add_system = false;
                }
            }
        }

        if (add_system) {
            cJSON *sys_msg = cJSON_CreateObject();
            if (sys_msg) {
                cJSON_AddStringToObject(sys_msg, "role", "system");
                cJSON_AddStringToObject(sys_msg, "content", system_role_copy);

                // Insert at the beginning of the array
                if (cJSON_GetArraySize(messages) > 0) {
                    cJSON_InsertItemInArray(messages, 0, sys_msg);
                } else {
                    cJSON_AddItemToArray(messages, sys_msg);
                }
            } else {
                ESP_LOGW(TAG, "Failed to create system message for prepending");
            }
        }
        free(system_role_copy);
    }

    esp_err_t result = perform_call_streaming(handle, messages, callback, user_data);
    return result;
}

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
                                     size_t response_size, int max_tool_iterations)
{
    if (!handle || !prompt || !response || response_size == 0) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    char *temp_response = NULL;

    // Check that tools are enabled
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    bool tools_enabled = handle->enable_tools;
    bool is_streaming = handle->enable_streaming;
    xSemaphoreGive(handle->mutex);

    if (!tools_enabled) {
        ESP_LOGE(TAG, "Tools are not enabled for this handle");
        return ESP_ERR_OPENROUTER_TOOLS_DISABLED;
    }

    if (is_streaming) {
        ESP_LOGE(TAG, "Handle configured for streaming - use openrouter_call_streaming_with_tools instead");
        return ESP_ERR_INVALID_STATE;
    }

    // Create initial messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        ESP_LOGE(TAG, "Failed to create messages array");
        return ESP_ERR_OPENROUTER_MEMORY;
    }

    err = ESP_OK;

    // Add system message if configured
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    char *system_role_copy = handle->system_role ? strdup(handle->system_role) : NULL;
    xSemaphoreGive(handle->mutex);

    if (system_role_copy) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (sys_msg) {
            cJSON_AddStringToObject(sys_msg, "role", "system");
            cJSON_AddStringToObject(sys_msg, "content", system_role_copy);
            cJSON_AddItemToArray(messages, sys_msg);
        }
        free(system_role_copy);
    }

    // Add user message
    cJSON *user_msg = cJSON_CreateObject();
    if (user_msg) {
        cJSON_AddStringToObject(user_msg, "role", "user");
        cJSON_AddStringToObject(user_msg, "content", prompt);
        cJSON_AddItemToArray(messages, user_msg);
    } else {
        err = ESP_ERR_OPENROUTER_MEMORY;
        goto cleanup;
    }

    // Allocate temporary response buffer when needed
    temp_response = malloc(response_size);
    if (!temp_response) {
        err = ESP_ERR_OPENROUTER_MEMORY;
        goto cleanup;
    }

    // Tool calling loop
    int iteration = 0;

    while (max_tool_iterations == 0 || iteration < max_tool_iterations) {
        iteration++;

        // Make API call
        err = perform_call_non_streaming(handle, messages, temp_response, response_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "API call failed on iteration %d", iteration);
            break;
        }

        // Parse response to check for tool calls
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
        ESP_LOGI(TAG, "Raw response received (iteration %d): %s", iteration, temp_response);
#endif

        // Check if response starts with [TOOL_CALLS] - OpenRouter's custom format
        if (strncmp(temp_response, "[TOOL_CALLS]", 12) == 0) {
            // Extract the JSON array part after "[TOOL_CALLS] "
            char *tool_calls_json = temp_response + 13; // Skip "[TOOL_CALLS] "

            // Parse the tool calls array directly
            cJSON *tool_calls_array = cJSON_Parse(tool_calls_json);
            if (!tool_calls_array || !cJSON_IsArray(tool_calls_array)) {
                ESP_LOGE(TAG, "Failed to parse tool calls JSON on iteration %d", iteration);
                ESP_LOGE(TAG, "Tool calls JSON was: %s", tool_calls_json);
                err = ESP_ERR_OPENROUTER_JSON_PARSE;
                break;
            }

            // Create a mock assistant message for the conversation
            cJSON *assistant_msg = cJSON_CreateObject();
            if (assistant_msg) {
                cJSON_AddStringToObject(assistant_msg, "role", "assistant");
                cJSON_AddStringToObject(assistant_msg, "content", "");
                cJSON_AddItemToObject(assistant_msg, "tool_calls", cJSON_Duplicate(tool_calls_array, true));
                cJSON_AddItemToArray(messages, assistant_msg);
            }

            // Process each tool call
            int tool_call_count = cJSON_GetArraySize(tool_calls_array);
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
            ESP_LOGI(TAG, "Processing %d tool calls", tool_call_count);
#endif

            for (int i = 0; i < tool_call_count; i++) {
                cJSON *tool_call = cJSON_GetArrayItem(tool_calls_array, i);
                if (!tool_call)
                    continue;

                cJSON *id = cJSON_GetObjectItem(tool_call, "id");
                cJSON *function = cJSON_GetObjectItem(tool_call, "function");
                if (!id || !function)
                    continue;

                cJSON *name = cJSON_GetObjectItem(function, "name");
                cJSON *arguments = cJSON_GetObjectItem(function, "arguments");
                if (!name || !cJSON_IsString(name))
                    continue;

                const char *function_name = name->valuestring;
                const char *function_args = arguments && cJSON_IsString(arguments) ? arguments->valuestring : "{}";
                const char *tool_call_id = id->valuestring;
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
                ESP_LOGI(TAG, "Executing tool call: %s with args: %s", function_name, function_args);
#endif

                // Execute the function
                char *result = NULL;
                if (handle->tool_callback) {
                    result = handle->tool_callback(tool_call_id, function_name, function_args, handle->tool_user_data);
                } else {
                    result = execute_function_call(handle, function_name, function_args);
                }

                // Create tool response message
                cJSON *tool_msg = cJSON_CreateObject();
                if (tool_msg) {
                    cJSON_AddStringToObject(tool_msg, "role", "tool");
                    cJSON_AddStringToObject(tool_msg, "tool_call_id", tool_call_id);
                    if (result) {
                        cJSON_AddStringToObject(tool_msg, "content", result);
                        free(result);
                    } else {
                        cJSON_AddStringToObject(tool_msg, "content", "{\"error\": \"Function execution failed\"}");
                    }
                    cJSON_AddItemToArray(messages, tool_msg);
                }
            }

            cJSON_Delete(tool_calls_array);
            continue; // Continue the loop for next iteration
        }

        // If not tool calls, check if it's a regular OpenAI-style JSON response
        cJSON *response_json = cJSON_Parse(temp_response);
        if (response_json) {
            // Standard OpenAI format - process normally
            cJSON *choices = cJSON_GetObjectItem(response_json, "choices");
            bool has_tool_calls = false;

            if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
                cJSON *choice = cJSON_GetArrayItem(choices, 0);
                if (choice) {
                    cJSON *message = cJSON_GetObjectItem(choice, "message");
                    if (message) {
                        cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");
                        has_tool_calls =
                            (tool_calls && cJSON_IsArray(tool_calls) && cJSON_GetArraySize(tool_calls) > 0);

                        if (!has_tool_calls) {
                            // No tool calls, extract final response
                            cJSON *content = cJSON_GetObjectItem(message, "content");
                            if (content && cJSON_IsString(content) && content->valuestring) {
                                strncpy(response, content->valuestring, response_size - 1);
                                response[response_size - 1] = '\0';
                            } else {
                                strncpy(response, "No content in response", response_size - 1);
                                response[response_size - 1] = '\0';
                            }
                        }
                    }
                }
            }

            if (!has_tool_calls) {
                // No tool calls, we're done
                cJSON_Delete(response_json);
                break;
            }

// Process tool calls in standard format
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
            ESP_LOGI(TAG, "Processing tool calls on iteration %d", iteration);
#endif
            err = process_tool_calls(handle, response_json, messages);
            cJSON_Delete(response_json);

            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to process tool calls");
                break;
            }
        } else {
// Not JSON at all - treat as plain text response (like the programming joke)
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
            ESP_LOGI(TAG, "Received plain text response, treating as final answer");
#endif

            strncpy(response, temp_response, response_size - 1);
            response[response_size - 1] = '\0';
            break; // Final response received
        }
    }

    if (max_tool_iterations > 0 && iteration >= max_tool_iterations) {
        ESP_LOGW(TAG, "Reached maximum tool iterations (%d)", max_tool_iterations);
        strncpy(response, "[WARNING: Maximum tool call iterations reached]", response_size - 1);
        response[response_size - 1] = '\0';
    }

cleanup:
    if (temp_response) {
        free(temp_response);
    }
    cJSON_Delete(messages);
    return err;
}

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
                                        size_t response_size, int max_tool_iterations)
{
    if (!handle || !messages || !response || response_size == 0) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    if (!cJSON_IsArray(messages)) {
        ESP_LOGE(TAG, "Messages parameter must be a JSON array");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    char *temp_response = NULL;

    // Check that tools are enabled
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    bool tools_enabled = handle->enable_tools;
    bool is_streaming = handle->enable_streaming;
    xSemaphoreGive(handle->mutex);

    if (!tools_enabled) {
        ESP_LOGE(TAG, "Tools are not enabled for this handle");
        return ESP_ERR_OPENROUTER_TOOLS_DISABLED;
    }

    if (is_streaming) {
        ESP_LOGE(TAG, "Handle configured for streaming - use openrouter_call_ex_streaming_with_tools instead");
        return ESP_ERR_INVALID_STATE;
    }

    // Prepend system role if set and not already present
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    char *system_role_copy = handle->system_role ? strdup(handle->system_role) : NULL;
    xSemaphoreGive(handle->mutex);

    if (system_role_copy) {
        cJSON *first = cJSON_GetArrayItem(messages, 0);
        bool add_system = true;

        if (first) {
            cJSON *role = cJSON_GetObjectItem(first, "role");
            if (role && cJSON_IsString(role) && role->valuestring && strcmp(role->valuestring, "system") == 0) {
                add_system = false;
            }
        }

        if (add_system) {
            cJSON *sys_msg = cJSON_CreateObject();
            if (sys_msg) {
                cJSON_AddStringToObject(sys_msg, "role", "system");
                cJSON_AddStringToObject(sys_msg, "content", system_role_copy);
                // Insert at beginning
                cJSON_InsertItemInArray(messages, 0, sys_msg);
            }
        }
        free(system_role_copy);
    }

    // Allocate temporary response buffer
    temp_response = malloc(response_size);
    if (!temp_response) {
        return ESP_ERR_OPENROUTER_MEMORY;
    }

    err = ESP_OK;
    int iteration = 0;

    while (max_tool_iterations == 0 || iteration < max_tool_iterations) {
        iteration++;

        // Make API call
        err = perform_call_non_streaming(handle, messages, temp_response, response_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "API call failed on iteration %d", iteration);
            break;
        }

// Parse response to check for tool calls
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
        ESP_LOGI(TAG, "Raw response received (ex iteration %d): %s", iteration, temp_response);
#endif
        // Check if response starts with [TOOL_CALLS] - OpenRouter's custom format
        if (strncmp(temp_response, "[TOOL_CALLS]", 12) == 0) {
            // Extract the JSON array part after "[TOOL_CALLS] "
            char *tool_calls_json = temp_response + 13; // Skip "[TOOL_CALLS] "

            // Parse the tool calls array directly
            cJSON *tool_calls_array = cJSON_Parse(tool_calls_json);
            if (!tool_calls_array || !cJSON_IsArray(tool_calls_array)) {
                ESP_LOGE(TAG, "Failed to parse tool calls JSON on ex iteration %d", iteration);
                ESP_LOGE(TAG, "Tool calls JSON was: %s", tool_calls_json);
                err = ESP_ERR_OPENROUTER_JSON_PARSE;
                break;
            }

            // Create a mock assistant message for the conversation
            cJSON *assistant_msg = cJSON_CreateObject();
            if (assistant_msg) {
                cJSON_AddStringToObject(assistant_msg, "role", "assistant");
                cJSON_AddStringToObject(assistant_msg, "content", "");
                cJSON_AddItemToObject(assistant_msg, "tool_calls", cJSON_Duplicate(tool_calls_array, true));
                cJSON_AddItemToArray(messages, assistant_msg);
            }

            // Process each tool call
            int tool_call_count = cJSON_GetArraySize(tool_calls_array);
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
            ESP_LOGI(TAG, "Processing %d tool calls", tool_call_count);
#endif

            for (int i = 0; i < tool_call_count; i++) {
                cJSON *tool_call = cJSON_GetArrayItem(tool_calls_array, i);
                if (!tool_call)
                    continue;

                cJSON *id = cJSON_GetObjectItem(tool_call, "id");
                cJSON *function = cJSON_GetObjectItem(tool_call, "function");
                if (!id || !function)
                    continue;

                cJSON *name = cJSON_GetObjectItem(function, "name");
                cJSON *arguments = cJSON_GetObjectItem(function, "arguments");
                if (!name || !cJSON_IsString(name))
                    continue;

                const char *function_name = name->valuestring;
                const char *function_args = arguments && cJSON_IsString(arguments) ? arguments->valuestring : "{}";
                const char *tool_call_id = id->valuestring;
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
                ESP_LOGI(TAG, "Executing tool call: %s with args: %s", function_name, function_args);
#endif

                // Execute the function
                char *result = NULL;
                if (handle->tool_callback) {
                    result = handle->tool_callback(tool_call_id, function_name, function_args, handle->tool_user_data);
                } else {
                    result = execute_function_call(handle, function_name, function_args);
                }

                // Create tool response message
                cJSON *tool_msg = cJSON_CreateObject();
                if (tool_msg) {
                    cJSON_AddStringToObject(tool_msg, "role", "tool");
                    cJSON_AddStringToObject(tool_msg, "tool_call_id", tool_call_id);
                    if (result) {
                        cJSON_AddStringToObject(tool_msg, "content", result);
                        free(result);
                    } else {
                        cJSON_AddStringToObject(tool_msg, "content", "{\"error\": \"Function execution failed\"}");
                    }
                    cJSON_AddItemToArray(messages, tool_msg);
                }
            }

            cJSON_Delete(tool_calls_array);
            continue; // Continue the loop for next iteration
        }

        // If not tool calls, check if it's a regular OpenAI-style JSON response
        cJSON *response_json = cJSON_Parse(temp_response);
        if (response_json) {
            // Standard OpenAI format - process normally
            cJSON *choices = cJSON_GetObjectItem(response_json, "choices");
            bool has_tool_calls = false;

            if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
                cJSON *choice = cJSON_GetArrayItem(choices, 0);
                if (choice) {
                    cJSON *message = cJSON_GetObjectItem(choice, "message");
                    if (message) {
                        cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");
                        has_tool_calls =
                            (tool_calls && cJSON_IsArray(tool_calls) && cJSON_GetArraySize(tool_calls) > 0);

                        if (!has_tool_calls) {
                            // No tool calls, extract final response
                            cJSON *content = cJSON_GetObjectItem(message, "content");
                            if (content && cJSON_IsString(content) && content->valuestring) {
                                strncpy(response, content->valuestring, response_size - 1);
                                response[response_size - 1] = '\0';
                            } else {
                                strncpy(response, "No content in response", response_size - 1);
                                response[response_size - 1] = '\0';
                            }
                        }
                    }
                }
            }

            if (!has_tool_calls) {
                // No tool calls, we're done
                cJSON_Delete(response_json);
                break;
            }

// Process tool calls in standard format
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
            ESP_LOGI(TAG, "Processing tool calls on iteration %d", iteration);
#endif
            err = process_tool_calls(handle, response_json, messages);
            cJSON_Delete(response_json);

            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to process tool calls");
                break;
            }
        } else {
// Not JSON at all - treat as plain text response
#if CONFIG_OPENROUTER_VERBOSE_LOGGING
            ESP_LOGI(TAG, "Received plain text response, treating as final answer");
#endif
            strncpy(response, temp_response, response_size - 1);
            response[response_size - 1] = '\0';
            break; // Final response received
        }
    }

    if (max_tool_iterations > 0 && iteration >= max_tool_iterations) {
        ESP_LOGW(TAG, "Reached maximum tool iterations (%d)", max_tool_iterations);
        strncpy(response, "[WARNING: Maximum tool call iterations reached]", response_size - 1);
        response[response_size - 1] = '\0';
    }

    if (temp_response) {
        free(temp_response);
    }
    return err;
}

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
                                               openrouter_stream_callback_t callback, void *user_data)
{
    if (!handle || !prompt || !callback) {
        ESP_LOGE(TAG, "Invalid arguments for streaming call with tools");
        return ESP_ERR_INVALID_ARG;
    }

    // Check that tools are enabled and streaming is enabled
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    bool tools_enabled = handle->enable_tools;
    bool is_streaming = handle->enable_streaming;
    xSemaphoreGive(handle->mutex);

    if (!tools_enabled) {
        ESP_LOGE(TAG, "Tools are not enabled for this handle");
        return ESP_ERR_OPENROUTER_TOOLS_DISABLED;
    }

    if (!is_streaming) {
        ESP_LOGE(TAG,
                 "Handle not configured for streaming - use openrouter_call_with_tools instead or enable streaming");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        ESP_LOGE(TAG, "Failed to create messages array");
        return ESP_ERR_OPENROUTER_MEMORY;
    }

    esp_err_t err = ESP_OK;

    // Add system message if configured
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    char *system_role_copy = handle->system_role ? strdup(handle->system_role) : NULL;
    xSemaphoreGive(handle->mutex);

    if (system_role_copy) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (sys_msg) {
            cJSON_AddStringToObject(sys_msg, "role", "system");
            cJSON_AddStringToObject(sys_msg, "content", system_role_copy);
            cJSON_AddItemToArray(messages, sys_msg);
        }
        free(system_role_copy);
    }

    // Add user message
    cJSON *user_msg = cJSON_CreateObject();
    if (user_msg) {
        cJSON_AddStringToObject(user_msg, "role", "user");
        cJSON_AddStringToObject(user_msg, "content", prompt);
        cJSON_AddItemToArray(messages, user_msg);
    } else {
        err = ESP_ERR_OPENROUTER_MEMORY;
        goto cleanup;
    }

    // Make the streaming API call
    err = perform_call_streaming(handle, messages, callback, user_data);

cleanup:
    cJSON_Delete(messages);
    return err;
}

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
                                                  openrouter_stream_callback_t callback, void *user_data)
{
    if (!handle || !messages || !callback) {
        ESP_LOGE(TAG, "Invalid arguments for streaming call_ex with tools");
        return ESP_ERR_INVALID_ARG;
    }

    if (!cJSON_IsArray(messages)) {
        ESP_LOGE(TAG, "Messages parameter must be a JSON array");
        return ESP_ERR_INVALID_ARG;
    }

    // Check that tools are enabled and streaming is enabled
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    bool tools_enabled = handle->enable_tools;
    bool is_streaming = handle->enable_streaming;
    xSemaphoreGive(handle->mutex);

    if (!tools_enabled) {
        ESP_LOGE(TAG, "Tools are not enabled for this handle");
        return ESP_ERR_OPENROUTER_TOOLS_DISABLED;
    }

    if (!is_streaming) {
        ESP_LOGE(TAG,
                 "Handle not configured for streaming - use openrouter_call_ex_with_tools instead or enable streaming");
        return ESP_ERR_INVALID_STATE;
    }

    // Prepend system role if set and not already present
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    char *system_role_copy = handle->system_role ? strdup(handle->system_role) : NULL;
    xSemaphoreGive(handle->mutex);

    if (system_role_copy) {
        cJSON *first = cJSON_GetArrayItem(messages, 0);
        bool add_system = true;

        if (first) {
            cJSON *role = cJSON_GetObjectItem(first, "role");
            if (role && cJSON_IsString(role) && role->valuestring && strcmp(role->valuestring, "system") == 0) {
                add_system = false;
            }
        }

        if (add_system) {
            cJSON *sys_msg = cJSON_CreateObject();
            if (sys_msg) {
                cJSON_AddStringToObject(sys_msg, "role", "system");
                cJSON_AddStringToObject(sys_msg, "content", system_role_copy);
                // Insert at beginning
                cJSON_InsertItemInArray(messages, 0, sys_msg);
            }
        }
        free(system_role_copy);
    }

    esp_err_t result = perform_call_streaming(handle, messages, callback, user_data);
    return result;
}


esp_err_t openrouter_call_with_image(openrouter_handle_t handle, const char *prompt, const char *image_path,
                                     char *response, size_t response_size)
{
    if (!handle || !prompt || !image_path || !response || response_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->enable_streaming) {
        ESP_LOGE(TAG, "Handle configured for streaming - use openrouter_call_with_image_streaming instead");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg = create_multimodal_message(prompt, image_path, NULL, true, false, false, false, 0, 0);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing API
    ret = openrouter_call_ex(handle, messages, response, response_size);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

esp_err_t openrouter_call_with_image_url(openrouter_handle_t handle, const char *prompt, const char *image_url,
                                         char *response, size_t response_size)
{
    if (!handle || !prompt || !image_url || !response || response_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->enable_streaming) {
        ESP_LOGE(TAG, "Handle configured for streaming - use openrouter_call_with_image_url_streaming instead");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg = create_multimodal_message(prompt, image_url, NULL, false, false, false, false, 0, 0);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing API
    ret = openrouter_call_ex(handle, messages, response, response_size);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

esp_err_t openrouter_call_with_image_data(openrouter_handle_t handle, const char *prompt,
                                          const unsigned char *image_data, size_t image_size, char *response,
                                          size_t response_size)
{
    if (!handle || !prompt || !image_data || image_size == 0 || !response || response_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->enable_streaming) {
        ESP_LOGE(TAG, "Handle configured for streaming - use openrouter_call_with_image_data_streaming instead");
        return ESP_ERR_INVALID_STATE;
    }

    if (image_size > MAX_IMAGE_FILE_SIZE) {
        ESP_LOGE(TAG, "Image data too large: %zu bytes (max: %d)", image_size, MAX_IMAGE_FILE_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg =
        create_multimodal_message(prompt, (const char *) image_data, NULL, false, false, true, false, image_size, 0);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing API
    ret = openrouter_call_ex(handle, messages, response, response_size);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

esp_err_t openrouter_call_with_audio(openrouter_handle_t handle, const char *prompt, const char *audio_path,
                                     char *response, size_t response_size)
{
    if (!handle || !prompt || !audio_path || !response || response_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->enable_streaming) {
        ESP_LOGE(TAG, "Handle configured for streaming - use openrouter_call_with_audio_streaming instead");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg = create_multimodal_message(prompt, NULL, audio_path, false, true, false, false, 0, 0);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing API
    ret = openrouter_call_ex(handle, messages, response, response_size);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

esp_err_t openrouter_call_with_audio_url(openrouter_handle_t handle, const char *prompt, const char *audio_url,
                                         char *response, size_t response_size)
{
    if (!handle || !prompt || !audio_url || !response || response_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->enable_streaming) {
        ESP_LOGE(TAG, "Handle configured for streaming - use openrouter_call_with_audio_url_streaming instead");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg = create_multimodal_message(prompt, NULL, audio_url, false, false, false, false, 0, 0);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing API
    ret = openrouter_call_ex(handle, messages, response, response_size);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

esp_err_t openrouter_call_with_audio_data(openrouter_handle_t handle, const char *prompt,
                                          const unsigned char *audio_data, size_t audio_size, char *response,
                                          size_t response_size)
{
    if (!handle || !prompt || !audio_data || audio_size == 0 || !response || response_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->enable_streaming) {
        ESP_LOGE(TAG, "Handle configured for streaming - use openrouter_call_with_audio_data_streaming instead");
        return ESP_ERR_INVALID_STATE;
    }

    if (audio_size > MAX_AUDIO_FILE_SIZE) {
        ESP_LOGE(TAG, "Audio data too large: %zu bytes (max: %d)", audio_size, MAX_AUDIO_FILE_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg =
        create_multimodal_message(prompt, NULL, (const char *) audio_data, false, false, false, true, 0, audio_size);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing API
    ret = openrouter_call_ex(handle, messages, response, response_size);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

esp_err_t openrouter_call_with_media(openrouter_handle_t handle, const char *prompt, const char *image_path,
                                     const char *audio_path, char *response, size_t response_size)
{
    if (!handle || !prompt || !response || response_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!image_path && !audio_path) {
        ESP_LOGE(TAG, "At least one media file must be provided");
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->enable_streaming) {
        ESP_LOGE(TAG, "Handle configured for streaming - use openrouter_call_with_media_streaming instead");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg = create_multimodal_message(prompt, image_path, audio_path, image_path != NULL, audio_path != NULL,
                                                false, false, 0, 0);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing API
    ret = openrouter_call_ex(handle, messages, response, response_size);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

/* ================================================================================
 * STREAMING API FUNCTIONS
 * ================================================================================ */

esp_err_t openrouter_call_with_image_streaming(openrouter_handle_t handle, const char *prompt, const char *image_path,
                                               openrouter_stream_callback_t callback, void *user_data)
{
    if (!handle || !prompt || !image_path || !callback) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->enable_streaming) {
        ESP_LOGE(TAG,
                 "Handle not configured for streaming - use openrouter_call_with_image instead or enable streaming");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg = create_multimodal_message(prompt, image_path, NULL, true, false, false, false, 0, 0);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing streaming API
    ret = openrouter_call_ex_streaming(handle, messages, callback, user_data);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

esp_err_t openrouter_call_with_image_url_streaming(openrouter_handle_t handle, const char *prompt,
                                                   const char *image_url, openrouter_stream_callback_t callback,
                                                   void *user_data)
{
    if (!handle || !prompt || !image_url || !callback) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->enable_streaming) {
        ESP_LOGE(
            TAG,
            "Handle not configured for streaming - use openrouter_call_with_image_url instead or enable streaming");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg = create_multimodal_message(prompt, image_url, NULL, false, false, false, false, 0, 0);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing streaming API
    ret = openrouter_call_ex_streaming(handle, messages, callback, user_data);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

esp_err_t openrouter_call_with_image_data_streaming(openrouter_handle_t handle, const char *prompt,
                                                    const unsigned char *image_data, size_t image_size,
                                                    openrouter_stream_callback_t callback, void *user_data)
{
    if (!handle || !prompt || !image_data || image_size == 0 || !callback) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->enable_streaming) {
        ESP_LOGE(
            TAG,
            "Handle not configured for streaming - use openrouter_call_with_image_data instead or enable streaming");
        return ESP_ERR_INVALID_STATE;
    }

    if (image_size > MAX_IMAGE_FILE_SIZE) {
        ESP_LOGE(TAG, "Image data too large: %zu bytes (max: %d)", image_size, MAX_IMAGE_FILE_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg =
        create_multimodal_message(prompt, (const char *) image_data, NULL, false, false, true, false, image_size, 0);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing streaming API
    ret = openrouter_call_ex_streaming(handle, messages, callback, user_data);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

esp_err_t openrouter_call_with_audio_streaming(openrouter_handle_t handle, const char *prompt, const char *audio_path,
                                               openrouter_stream_callback_t callback, void *user_data)
{
    if (!handle || !prompt || !audio_path || !callback) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->enable_streaming) {
        ESP_LOGE(TAG,
                 "Handle not configured for streaming - use openrouter_call_with_audio instead or enable streaming");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg = create_multimodal_message(prompt, NULL, audio_path, false, true, false, false, 0, 0);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing streaming API
    ret = openrouter_call_ex_streaming(handle, messages, callback, user_data);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

esp_err_t openrouter_call_with_audio_url_streaming(openrouter_handle_t handle, const char *prompt,
                                                   const char *audio_url, openrouter_stream_callback_t callback,
                                                   void *user_data)
{
    if (!handle || !prompt || !audio_url || !callback) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->enable_streaming) {
        ESP_LOGE(
            TAG,
            "Handle not configured for streaming - use openrouter_call_with_audio_url instead or enable streaming");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg = create_multimodal_message(prompt, NULL, audio_url, false, false, false, false, 0, 0);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing streaming API
    ret = openrouter_call_ex_streaming(handle, messages, callback, user_data);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

esp_err_t openrouter_call_with_audio_data_streaming(openrouter_handle_t handle, const char *prompt,
                                                    const unsigned char *audio_data, size_t audio_size,
                                                    openrouter_stream_callback_t callback, void *user_data)
{
    if (!handle || !prompt || !audio_data || audio_size == 0 || !callback) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->enable_streaming) {
        ESP_LOGE(
            TAG,
            "Handle not configured for streaming - use openrouter_call_with_audio_data instead or enable streaming");
        return ESP_ERR_INVALID_STATE;
    }

    if (audio_size > MAX_AUDIO_FILE_SIZE) {
        ESP_LOGE(TAG, "Audio data too large: %zu bytes (max: %d)", audio_size, MAX_AUDIO_FILE_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg =
        create_multimodal_message(prompt, NULL, (const char *) audio_data, false, false, false, true, 0, audio_size);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing streaming API
    ret = openrouter_call_ex_streaming(handle, messages, callback, user_data);

cleanup:
    cJSON_Delete(messages);
    return ret;
}

esp_err_t openrouter_call_with_media_streaming(openrouter_handle_t handle, const char *prompt, const char *image_path,
                                               const char *audio_path, openrouter_stream_callback_t callback,
                                               void *user_data)
{
    if (!handle || !prompt || !callback) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!image_path && !audio_path) {
        ESP_LOGE(TAG, "At least one media file must be provided");
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->enable_streaming) {
        ESP_LOGE(TAG,
                 "Handle not configured for streaming - use openrouter_call_with_media instead or enable streaming");
        return ESP_ERR_INVALID_STATE;
    }

    // Create messages array
    cJSON *messages = cJSON_CreateArray();
    if (!messages) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    // Add system message if configured
    if (handle->system_role && strlen(handle->system_role) > 0) {
        cJSON *sys_msg = cJSON_CreateObject();
        if (!sys_msg) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", handle->system_role);
        cJSON_AddItemToArray(messages, sys_msg);
    }

    // Create multimodal message
    cJSON *user_msg = create_multimodal_message(prompt, image_path, audio_path, image_path != NULL, audio_path != NULL,
                                                false, false, 0, 0);
    if (!user_msg) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cJSON_AddItemToArray(messages, user_msg);

    // Make the call using existing streaming API
    ret = openrouter_call_ex_streaming(handle, messages, callback, user_data);

cleanup:
    cJSON_Delete(messages);
    return ret;
}
