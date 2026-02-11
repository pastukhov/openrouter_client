#pragma once

#include "openrouter_component.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace openrouter {

template<typename... Ts>
class AskAction : public Action<Ts...>, public Parented<OpenRouterComponent> {
 public:
  TEMPLATABLE_VALUE(std::string, prompt)

  void play(Ts... x) override {
    auto prompt = this->prompt_.value(x...);
    this->parent_->ask(prompt);
  }
};

template<typename... Ts>
class SetModelAction : public Action<Ts...>, public Parented<OpenRouterComponent> {
 public:
  TEMPLATABLE_VALUE(std::string, model)

  void play(Ts... x) override {
    auto model = this->model_.value(x...);
    this->parent_->update_model(model);
  }
};

template<typename... Ts>
class SetSystemRoleAction : public Action<Ts...>, public Parented<OpenRouterComponent> {
 public:
  TEMPLATABLE_VALUE(std::string, system_role)

  void play(Ts... x) override {
    auto system_role = this->system_role_.value(x...);
    this->parent_->update_system_role(system_role);
  }
};

}  // namespace openrouter
}  // namespace esphome
