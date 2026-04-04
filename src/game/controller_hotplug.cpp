#include "mm64_input_hotplug.hpp"

#include <algorithm>

ControllerState::ControllerState() : controller{}, latest_accelerometer{}, motion{}, prev_gyro_timestamp{} {
    motion.Reset();
    motion.SetCalibrationMode(
        GamepadMotionHelpers::CalibrationMode::Stillness
        | GamepadMotionHelpers::CalibrationMode::SensorFusion);
}

namespace mm64::input_hotplug {

void close_controller_handle(SDL_GameController* controller) {
    if (controller != nullptr) {
        SDL_GameControllerClose(controller);
    }
}

void reset_controller_state(ControllerState& state, SDL_GameController* controller) {
    state.controller = controller;
    state.latest_accelerometer = {};
    state.prev_gyro_timestamp = 0;
    state.motion.Reset();
    state.motion.SetCalibrationMode(
        GamepadMotionHelpers::CalibrationMode::Stillness
        | GamepadMotionHelpers::CalibrationMode::SensorFusion);
}

void rebuild_connected_controllers(std::vector<SDL_GameController*>& cur_controllers, const ControllerStateMap& controller_states) {
    cur_controllers.clear();

    for (const auto& [id, state] : controller_states) {
        (void)id;
        if (state.controller != nullptr) {
            cur_controllers.push_back(state.controller);
        }
    }
}

void handle_controller_added(ControllerStateMap& controller_states, std::vector<SDL_GameController*>& cur_controllers, SDL_GameController* controller, SDL_JoystickID instance_id) {
    ControllerState& state = controller_states[instance_id];
    close_controller_handle(state.controller);
    reset_controller_state(state, controller);
    rebuild_connected_controllers(cur_controllers, controller_states);
}

void handle_controller_removed(ControllerStateMap& controller_states, std::vector<SDL_GameController*>& cur_controllers, SDL_JoystickID instance_id) {
    auto state_it = controller_states.find(instance_id);
    if (state_it != controller_states.end()) {
        close_controller_handle(state_it->second.controller);
        controller_states.erase(state_it);
    }

    rebuild_connected_controllers(cur_controllers, controller_states);
}

bool has_connected_controller(const std::vector<SDL_GameController*>& cur_controllers, const ControllerStateMap& controller_states) {
    if (!cur_controllers.empty()) {
        return true;
    }

    return std::any_of(controller_states.begin(), controller_states.end(), [](const auto& entry) {
        return entry.second.controller != nullptr;
    });
}

} // namespace mm64::input_hotplug
