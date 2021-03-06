#pragma once

#include "manipulator/details/base.hpp"
#include "manipulator/details/types.hpp"
#include <json/json.hpp>
#include <vector>

namespace krbn {
namespace manipulator {
namespace details {
class basic final : public base {
public:
  basic(const nlohmann::json& json) : base(),
                                      from_(json.find("from") != std::end(json) ? json["from"] : nlohmann::json()) {
    {
      const std::string key = "to";
      if (json.find(key) != std::end(json) && json[key].is_array()) {
        for (const auto& j : json[key]) {
          to_.emplace_back(j);
        }
      }
    }
  }

  basic(const event_definition& from,
        const event_definition& to) : from_(from),
                                      to_({to}) {
  }

  virtual ~basic(void) {
  }

  virtual void manipulate(event_queue::queued_event& front_input_event,
                          const event_queue& input_event_queue,
                          event_queue& output_event_queue,
                          uint64_t time_stamp) {
    if (!front_input_event.get_valid()) {
      return;
    }

    bool is_target = false;

    if (auto key_code = front_input_event.get_event().get_key_code()) {
      if (from_.get_key_code() == key_code) {
        is_target = true;
      }
    }
    if (auto pointing_button = front_input_event.get_event().get_pointing_button()) {
      if (from_.get_pointing_button() == pointing_button) {
        is_target = true;
      }
    }

    if (is_target) {
      if (front_input_event.get_event_type() == event_type::key_down) {

        if (!valid_) {
          is_target = false;
        }

        if (!from_.test_modifiers(output_event_queue.get_modifier_flag_manager())) {
          is_target = false;
        }

        if (is_target) {
          manipulated_original_events_.emplace_back(front_input_event.get_device_id(),
                                                    front_input_event.get_original_event());
        }

      } else {
        // event_type::key_up

        // Check original_event in order to determine the correspond key_down is manipulated.

        auto it = std::find_if(std::begin(manipulated_original_events_),
                               std::end(manipulated_original_events_),
                               [&](const auto& pair) {
                                 return pair.first == front_input_event.get_device_id() &&
                                        pair.second == front_input_event.get_original_event();
                               });
        if (it != std::end(manipulated_original_events_)) {
          manipulated_original_events_.erase(it);
        } else {
          is_target = false;
        }
      }

      if (is_target) {
        front_input_event.set_valid(false);

        uint64_t to_time_stamp = front_input_event.get_time_stamp();

        for (size_t i = 0; i < to_.size(); ++i) {
          if (auto event = to_[i].to_event()) {
            if (front_input_event.get_event_type() == event_type::key_down) {
              output_event_queue.emplace_back_event(front_input_event.get_device_id(),
                                                    to_time_stamp + i,
                                                    *event,
                                                    event_type::key_down,
                                                    front_input_event.get_original_event());

              if (i != to_.size() - 1) {
                output_event_queue.emplace_back_event(front_input_event.get_device_id(),
                                                      to_time_stamp + i,
                                                      *event,
                                                      event_type::key_up,
                                                      front_input_event.get_original_event());
              }

            } else {
              // event_type::key_up

              if (i == to_.size() - 1) {
                output_event_queue.emplace_back_event(front_input_event.get_device_id(),
                                                      to_time_stamp + i,
                                                      *event,
                                                      event_type::key_up,
                                                      front_input_event.get_original_event());
              }
            }
          }
        }

        output_event_queue.increase_time_stamp_delay(to_.size() - 1);
      }
    }
  }

  virtual bool active(void) const {
    return !manipulated_original_events_.empty();
  }

  virtual void inactivate_by_device_id(device_id device_id,
                                       event_queue& output_event_queue,
                                       uint64_t time_stamp) {
    while (true) {
      auto it = std::find_if(std::begin(manipulated_original_events_),
                             std::end(manipulated_original_events_),
                             [&](const auto& pair) {
                               return pair.first == device_id;
                             });
      if (it == std::end(manipulated_original_events_)) {
        break;
      }

      if (to_.size() > 0) {
        if (auto event = to_.back().to_event()) {
          output_event_queue.emplace_back_event(device_id,
                                                time_stamp,
                                                *event,
                                                event_type::key_up,
                                                it->second);
        }
      }

      manipulated_original_events_.erase(it);
    }
  }

  const event_definition& get_from(void) const {
    return from_;
  }

  const std::vector<event_definition>& get_to(void) const {
    return to_;
  }

private:
  event_definition from_;
  std::vector<event_definition> to_;

  std::vector<std::pair<device_id, event_queue::queued_event::event>> manipulated_original_events_;
};
} // namespace details
} // namespace manipulator
} // namespace krbn
