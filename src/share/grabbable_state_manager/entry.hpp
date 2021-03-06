#pragma once

class entry final {
public:
  entry(registry_entry_id registry_entry_id) : grabbable_state_(registry_entry_id,
                                                                grabbable_state::state::none,
                                                                grabbable_state::ungrabbable_temporarily_reason::none,
                                                                0) {
  }

  const grabbable_state& get_grabbable_state(void) const {
    return grabbable_state_;
  }

  void set_grabbable_state(grabbable_state grabbable_state) {
    grabbable_state_ = grabbable_state;
  }

  void update(registry_entry_id registry_entry_id,
              uint64_t time_stamp,
              const event_queue::queued_event& queued_event) {
    if (auto key_code = queued_event.get_event().get_key_code()) {
      if (auto hid_usage_page = types::make_hid_usage_page(*key_code)) {
        if (auto hid_usage = types::make_hid_usage(*key_code)) {
          keyboard_repeat_detector_.set(*hid_usage_page,
                                        *hid_usage,
                                        queued_event.get_event_type());

          if (auto m = types::make_modifier_flag(*hid_usage_page,
                                                 *hid_usage)) {
            if (queued_event.get_event_type() == event_type::key_down) {
              pressed_modifier_flags_.insert(*m);
            } else if (queued_event.get_event_type() == event_type::key_up) {
              pressed_modifier_flags_.erase(*m);
            }
          }
        }
      }
    }

    if (auto consumer_key_code = queued_event.get_event().get_consumer_key_code()) {
      if (auto hid_usage_page = types::make_hid_usage_page(*consumer_key_code)) {
        if (auto hid_usage = types::make_hid_usage(*consumer_key_code)) {
          keyboard_repeat_detector_.set(*hid_usage_page,
                                        *hid_usage,
                                        queued_event.get_event_type());
        }
      }
    }

    if (auto pointing_button = queued_event.get_event().get_pointing_button()) {
      if (queued_event.get_event_type() == event_type::key_down) {
        pressed_pointing_buttons_.insert(*pointing_button);
      } else if (queued_event.get_event_type() == event_type::key_up) {
        pressed_pointing_buttons_.erase(*pointing_button);
      }
    }

    update_grabbable_state(registry_entry_id, time_stamp);
  }

private:
  void update_grabbable_state(registry_entry_id registry_entry_id,
                              uint64_t time_stamp) {
    // Ungrabbable while key repeating

    if (keyboard_repeat_detector_.is_repeating()) {
      grabbable_state_ = grabbable_state(registry_entry_id,
                                         grabbable_state::state::ungrabbable_temporarily,
                                         grabbable_state::ungrabbable_temporarily_reason::key_repeating,
                                         time_stamp);
      return;
    }

    // Ungrabbable while modifier keys are pressed
    //
    // We have to check the modifier keys state to avoid pressed physical modifiers affects in mouse events.
    // (See DEVELOPMENT.md > Modifier flags handling in kernel)

    if (!pressed_modifier_flags_.empty()) {
      grabbable_state_ = grabbable_state(registry_entry_id,
                                         grabbable_state::state::ungrabbable_temporarily,
                                         grabbable_state::ungrabbable_temporarily_reason::modifier_key_pressed,
                                         time_stamp);
      return;
    }

    // Ungrabbable while pointing button is pressed.
    //
    // We should not grab the device while a button is pressed since we cannot release the button.
    // (To release the button, we have to send a hid report to the device. But we cannot do it.)

    if (!pressed_pointing_buttons_.empty()) {
      grabbable_state_ = grabbable_state(registry_entry_id,
                                         grabbable_state::state::ungrabbable_temporarily,
                                         grabbable_state::ungrabbable_temporarily_reason::pointing_button_pressed,
                                         time_stamp);
      return;
    }

    grabbable_state_ = grabbable_state(registry_entry_id,
                                       grabbable_state::state::grabbable,
                                       grabbable_state::ungrabbable_temporarily_reason::none,
                                       time_stamp);
  }

  keyboard_repeat_detector keyboard_repeat_detector_;
  std::unordered_set<modifier_flag> pressed_modifier_flags_;
  std::unordered_set<pointing_button> pressed_pointing_buttons_;
  grabbable_state grabbable_state_;
};
