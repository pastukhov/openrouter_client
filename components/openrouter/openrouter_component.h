#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <string>
#include <queue>
#include <functional>

extern "C" {
#include "openrouter.h"
}

namespace esphome {
namespace openrouter {

class OpenRouterComponent;

class OpenRouterTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void set_parent(OpenRouterComponent *parent) { this->parent_ = parent; }
  void set_publish_streaming(bool publish_streaming) { this->publish_streaming_ = publish_streaming; }
  bool get_publish_streaming() const { return this->publish_streaming_; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

 protected:
  OpenRouterComponent *parent_{nullptr};
  bool publish_streaming_{false};
};

class OpenRouterComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  // Configuration setters
  void set_api_key(const std::string &api_key) { this->api_key_ = api_key; }
  void set_model(const std::string &model) { this->model_ = model; }
  void set_temperature(float temperature) { this->temperature_ = temperature; }
  void set_max_tokens(int max_tokens) { this->max_tokens_ = max_tokens; }
  void set_system_role(const std::string &system_role) { this->system_role_ = system_role; }
  void set_enable_streaming(bool enable_streaming) { this->enable_streaming_ = enable_streaming; }
  void set_http_timeout(uint32_t http_timeout) { this->http_timeout_ = http_timeout; }
  void set_response_buffer_size(size_t response_buffer_size) { this->response_buffer_size_ = response_buffer_size; }

  // Runtime setters (can be called from actions)
  void update_model(const std::string &model);
  void update_system_role(const std::string &system_role);

  // Main action
  void ask(const std::string &prompt);

  // Trigger registration
  void add_on_response_callback(std::function<void(const std::string &)> callback) {
    this->on_response_callbacks_.push_back(std::move(callback));
  }
  void add_on_error_callback(std::function<void(const std::string &)> callback) {
    this->on_error_callbacks_.push_back(std::move(callback));
  }
  void add_on_streaming_chunk_callback(std::function<void(const std::string &)> callback) {
    this->on_streaming_chunk_callbacks_.push_back(std::move(callback));
  }

  // Text sensor registration
  void register_text_sensor(OpenRouterTextSensor *sensor) { this->text_sensors_.push_back(sensor); }

 protected:
  // Configuration
  std::string api_key_;
  std::string model_;
  float temperature_{0.7f};
  int max_tokens_{1024};
  std::string system_role_;
  bool enable_streaming_{false};
  uint32_t http_timeout_{30000};
  size_t response_buffer_size_{4096};

  // OpenRouter handle
  openrouter_handle_t handle_{nullptr};

  // Request queue item
  struct RequestItem {
    std::string prompt;
  };

  // Async task management
  std::queue<RequestItem> request_queue_;
  TaskHandle_t task_handle_{nullptr};
  SemaphoreHandle_t mutex_{nullptr};

  // Response handling
  std::string pending_response_;
  std::string pending_error_;
  std::string streaming_buffer_;
  bool response_ready_{false};
  bool error_ready_{false};
  bool streaming_chunk_ready_{false};
  bool is_processing_{false};

  // Callbacks
  std::vector<std::function<void(const std::string &)>> on_response_callbacks_;
  std::vector<std::function<void(const std::string &)>> on_error_callbacks_;
  std::vector<std::function<void(const std::string &)>> on_streaming_chunk_callbacks_;

  // Text sensors
  std::vector<OpenRouterTextSensor *> text_sensors_;

  // Internal methods
  void create_handle_();
  void process_next_request_();
  static void task_function_(void *param);
  static void streaming_callback_(const char *content, bool is_complete, void *user_data);

  void fire_response_(const std::string &response);
  void fire_error_(const std::string &error);
  void fire_streaming_chunk_(const std::string &chunk);
};

// Triggers
class ResponseTrigger : public Trigger<std::string> {
 public:
  explicit ResponseTrigger(OpenRouterComponent *parent) {
    parent->add_on_response_callback([this](const std::string &response) {
      this->trigger(response);
    });
  }
};

class ErrorTrigger : public Trigger<std::string> {
 public:
  explicit ErrorTrigger(OpenRouterComponent *parent) {
    parent->add_on_error_callback([this](const std::string &error) {
      this->trigger(error);
    });
  }
};

class StreamingChunkTrigger : public Trigger<std::string> {
 public:
  explicit StreamingChunkTrigger(OpenRouterComponent *parent) {
    parent->add_on_streaming_chunk_callback([this](const std::string &chunk) {
      this->trigger(chunk);
    });
  }
};

}  // namespace openrouter
}  // namespace esphome
