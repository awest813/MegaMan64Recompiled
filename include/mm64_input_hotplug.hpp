#ifndef MM64_INPUT_HOTPLUG_HPP
#define MM64_INPUT_HOTPLUG_HPP

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "SDL.h"
#include "GamepadMotion.hpp"

struct ControllerState {
    SDL_GameController* controller;
    std::array<float, 3> latest_accelerometer;
    GamepadMotion motion;
    uint32_t prev_gyro_timestamp;

    ControllerState();
};

namespace mm64::input_hotplug {

using ControllerStateMap = std::unordered_map<SDL_JoystickID, ControllerState>;

void close_controller_handle(SDL_GameController* controller);
void reset_controller_state(ControllerState& state, SDL_GameController* controller);
void rebuild_connected_controllers(std::vector<SDL_GameController*>& cur_controllers, const ControllerStateMap& controller_states);
void handle_controller_added(ControllerStateMap& controller_states, std::vector<SDL_GameController*>& cur_controllers, SDL_GameController* controller, SDL_JoystickID instance_id);
void handle_controller_removed(ControllerStateMap& controller_states, std::vector<SDL_GameController*>& cur_controllers, SDL_JoystickID instance_id);
bool has_connected_controller(const std::vector<SDL_GameController*>& cur_controllers, const ControllerStateMap& controller_states);

} // namespace mm64::input_hotplug

#endif
