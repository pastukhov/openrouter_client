#include "openrouter_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace openrouter {

static const char *const TAG = "openrouter";

// Task stack size and priority
static const uint32_t TASK_STACK_SIZE = 8192;
static const UBaseType_t TASK_PRIORITY = 5;

void OpenRouterTextSensor::setup() {
  if (this->parent_ != nullptr) {
    this->parent_->register_text_sensor(this);
  }
}

void OpenRouterComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up OpenRouter...");

  // Create mutex for thread-safe access
  this->mutex_ = xSemaphoreCreateMutex();
  if (this->mutex_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create mutex");
    this->mark_failed();
    return;
  }

  // Create the OpenRouter handle
  this->create_handle_();

  if (this->handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create OpenRouter handle");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "OpenRouter initialized successfully");
}

void OpenRouterComponent::create_handle_() {
  openrouter_config_t config = {};
  config.api_key = this->api_key_.c_str();
  config.default_model = this->model_.c_str();
  config.temperature = this->temperature_;
  config.max_tokens = this->max_tokens_;
  config.enable_streaming = this->enable_streaming_;
  config.http_timeout_ms = this->http_timeout_;
  config.response_buffer_size = this->response_buffer_size_;

  if (!this->system_role_.empty()) {
    config.default_system_role = this->system_role_.c_str();
  }

  this->handle_ = openrouter_create(&config);
}

void OpenRouterComponent::loop() {
  if (this->mutex_ == nullptr) {
    return;
  }

  // Check for pending response
  if (xSemaphoreTake(this->mutex_, 0) == pdTRUE) {
    // Handle streaming chunks
    if (this->streaming_chunk_ready_) {
      std::string chunk = std::move(this->streaming_buffer_);
      this->streaming_buffer_.clear();
      this->streaming_chunk_ready_ = false;
      xSemaphoreGive(this->mutex_);

      this->fire_streaming_chunk_(chunk);
      return;
    }

    // Handle complete response
    if (this->response_ready_) {
      std::string response = std::move(this->pending_response_);
      this->pending_response_.clear();
      this->response_ready_ = false;
      this->is_processing_ = false;
      xSemaphoreGive(this->mutex_);

      this->fire_response_(response);

      // Process next request if queued
      this->process_next_request_();
      return;
    }

    // Handle error
    if (this->error_ready_) {
      std::string error = std::move(this->pending_error_);
      this->pending_error_.clear();
      this->error_ready_ = false;
      this->is_processing_ = false;
      xSemaphoreGive(this->mutex_);

      this->fire_error_(error);

      // Process next request if queued
      this->process_next_request_();
      return;
    }

    xSemaphoreGive(this->mutex_);
  }
}

void OpenRouterComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenRouter:");
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_.c_str());
  ESP_LOGCONFIG(TAG, "  Temperature: %.2f", this->temperature_);
  ESP_LOGCONFIG(TAG, "  Max Tokens: %d", this->max_tokens_);
  ESP_LOGCONFIG(TAG, "  Streaming: %s", YESNO(this->enable_streaming_));
  ESP_LOGCONFIG(TAG, "  HTTP Timeout: %u ms", this->http_timeout_);
  ESP_LOGCONFIG(TAG, "  Response Buffer: %zu bytes", this->response_buffer_size_);
  if (!this->system_role_.empty()) {
    ESP_LOGCONFIG(TAG, "  System Role: (configured)");
  }
}

void OpenRouterComponent::update_model(const std::string &model) {
  if (xSemaphoreTake(this->mutex_, portMAX_DELAY) == pdTRUE) {
    this->model_ = model;
    if (this->handle_ != nullptr) {
      openrouter_set_model(this->handle_, model.c_str());
    }
    xSemaphoreGive(this->mutex_);
    ESP_LOGI(TAG, "Model updated to: %s", model.c_str());
  }
}

void OpenRouterComponent::update_system_role(const std::string &system_role) {
  if (xSemaphoreTake(this->mutex_, portMAX_DELAY) == pdTRUE) {
    this->system_role_ = system_role;
    if (this->handle_ != nullptr) {
      openrouter_set_system_role(this->handle_, system_role.c_str());
    }
    xSemaphoreGive(this->mutex_);
    ESP_LOGI(TAG, "System role updated");
  }
}

void OpenRouterComponent::ask(const std::string &prompt) {
  if (this->handle_ == nullptr) {
    ESP_LOGE(TAG, "OpenRouter handle not initialized");
    return;
  }

  if (xSemaphoreTake(this->mutex_, portMAX_DELAY) == pdTRUE) {
    RequestItem item;
    item.prompt = prompt;
    this->request_queue_.push(std::move(item));
    xSemaphoreGive(this->mutex_);
  }

  ESP_LOGI(TAG, "Request queued: %.50s...", prompt.c_str());
  this->process_next_request_();
}

void OpenRouterComponent::process_next_request_() {
  if (xSemaphoreTake(this->mutex_, 0) != pdTRUE) {
    return;
  }

  if (this->is_processing_ || this->request_queue_.empty()) {
    xSemaphoreGive(this->mutex_);
    return;
  }

  this->is_processing_ = true;
  xSemaphoreGive(this->mutex_);

  // Create task to process request
  BaseType_t result = xTaskCreate(
    task_function_,
    "openrouter_task",
    TASK_STACK_SIZE,
    this,
    TASK_PRIORITY,
    &this->task_handle_
  );

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create task");
    if (xSemaphoreTake(this->mutex_, portMAX_DELAY) == pdTRUE) {
      this->is_processing_ = false;
      this->pending_error_ = "Failed to create task";
      this->error_ready_ = true;
      xSemaphoreGive(this->mutex_);
    }
  }
}

void OpenRouterComponent::task_function_(void *param) {
  OpenRouterComponent *self = static_cast<OpenRouterComponent *>(param);

  // Get the request from queue
  std::string prompt;
  if (xSemaphoreTake(self->mutex_, portMAX_DELAY) == pdTRUE) {
    if (!self->request_queue_.empty()) {
      prompt = std::move(self->request_queue_.front().prompt);
      self->request_queue_.pop();
    }
    xSemaphoreGive(self->mutex_);
  }

  if (prompt.empty()) {
    if (xSemaphoreTake(self->mutex_, portMAX_DELAY) == pdTRUE) {
      self->is_processing_ = false;
      xSemaphoreGive(self->mutex_);
    }
    vTaskDelete(nullptr);
    return;
  }

  ESP_LOGI(TAG, "Processing request...");

  esp_err_t err;

  if (self->enable_streaming_) {
    // Streaming call
    err = openrouter_call_streaming(
      self->handle_,
      prompt.c_str(),
      streaming_callback_,
      self
    );

    if (err != ESP_OK) {
      if (xSemaphoreTake(self->mutex_, portMAX_DELAY) == pdTRUE) {
        self->pending_error_ = "Streaming call failed: " + std::to_string(err);
        self->error_ready_ = true;
        xSemaphoreGive(self->mutex_);
      }
    }
  } else {
    // Non-streaming call
    char *response_buffer = (char *)malloc(self->response_buffer_size_);
    if (response_buffer == nullptr) {
      if (xSemaphoreTake(self->mutex_, portMAX_DELAY) == pdTRUE) {
        self->pending_error_ = "Failed to allocate response buffer";
        self->error_ready_ = true;
        xSemaphoreGive(self->mutex_);
      }
      vTaskDelete(nullptr);
      return;
    }

    err = openrouter_call(
      self->handle_,
      prompt.c_str(),
      response_buffer,
      self->response_buffer_size_
    );

    if (xSemaphoreTake(self->mutex_, portMAX_DELAY) == pdTRUE) {
      if (err == ESP_OK) {
        self->pending_response_ = response_buffer;
        self->response_ready_ = true;
      } else {
        self->pending_error_ = "API call failed: " + std::to_string(err);
        self->error_ready_ = true;
      }
      xSemaphoreGive(self->mutex_);
    }

    free(response_buffer);
  }

  vTaskDelete(nullptr);
}

void OpenRouterComponent::streaming_callback_(const char *content, bool is_complete, void *user_data) {
  OpenRouterComponent *self = static_cast<OpenRouterComponent *>(user_data);

  if (content == nullptr) {
    return;
  }

  if (xSemaphoreTake(self->mutex_, portMAX_DELAY) == pdTRUE) {
    if (is_complete) {
      // Stream finished - set complete response
      self->response_ready_ = true;
    } else if (strlen(content) > 0) {
      // Accumulate streaming content
      self->pending_response_ += content;

      // Also notify about chunk
      self->streaming_buffer_ = content;
      self->streaming_chunk_ready_ = true;
    }
    xSemaphoreGive(self->mutex_);
  }
}

void OpenRouterComponent::fire_response_(const std::string &response) {
  ESP_LOGI(TAG, "Response received (%zu chars)", response.length());

  // Publish to text sensors
  for (auto *sensor : this->text_sensors_) {
    sensor->publish_state(response);
  }

  // Fire callbacks
  for (auto &callback : this->on_response_callbacks_) {
    callback(response);
  }
}

void OpenRouterComponent::fire_error_(const std::string &error) {
  ESP_LOGE(TAG, "Error: %s", error.c_str());

  // Fire callbacks
  for (auto &callback : this->on_error_callbacks_) {
    callback(error);
  }
}

void OpenRouterComponent::fire_streaming_chunk_(const std::string &chunk) {
  ESP_LOGV(TAG, "Streaming chunk: %s", chunk.c_str());

  // Publish to text sensors with streaming enabled
  for (auto *sensor : this->text_sensors_) {
    if (sensor->get_publish_streaming()) {
      sensor->publish_state(chunk);
    }
  }

  // Fire callbacks
  for (auto &callback : this->on_streaming_chunk_callbacks_) {
    callback(chunk);
  }
}

}  // namespace openrouter
}  // namespace esphome
