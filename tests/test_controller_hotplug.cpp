#include <cstdlib>
#include <vector>

#define SDL_MAIN_HANDLED
#include "mm64_input_hotplug.hpp"

static int failures = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            failures++; \
            return; \
        } \
    } while (0)

namespace {

std::vector<SDL_GameController*> g_closed_controllers{};
const std::array<float, 3> g_zero_accelerometer{};

SDL_GameController* fake_controller(uintptr_t value) {
    return reinterpret_cast<SDL_GameController*>(value);
}

void reset_stub_state() {
    g_closed_controllers.clear();
}

} // namespace

extern "C" void SDL_GameControllerClose(SDL_GameController* gamecontroller) {
    g_closed_controllers.push_back(gamecontroller);
}

static void test_add_controller_marks_connected_and_tracks_handle() {
    reset_stub_state();

    mm64::input_hotplug::ControllerStateMap controller_states{};
    std::vector<SDL_GameController*> cur_controllers{};

    SDL_GameController* controller = fake_controller(0x1001);
    mm64::input_hotplug::handle_controller_added(controller_states, cur_controllers, controller, 7);

    CHECK(controller_states.size() == 1);
    CHECK(cur_controllers.size() == 1);
    CHECK(cur_controllers[0] == controller);
    CHECK(controller_states[7].controller == controller);
    CHECK(controller_states[7].latest_accelerometer == g_zero_accelerometer);
    CHECK(controller_states[7].prev_gyro_timestamp == 0);
    CHECK(mm64::input_hotplug::has_connected_controller(cur_controllers, controller_states));
    CHECK(g_closed_controllers.empty());
}

static void test_readding_same_instance_replaces_and_closes_old_handle() {
    reset_stub_state();

    mm64::input_hotplug::ControllerStateMap controller_states{};
    std::vector<SDL_GameController*> cur_controllers{};

    SDL_GameController* first = fake_controller(0x1001);
    SDL_GameController* second = fake_controller(0x1002);

    mm64::input_hotplug::handle_controller_added(controller_states, cur_controllers, first, 7);
    controller_states[7].latest_accelerometer = {1.0f, 2.0f, 3.0f};
    controller_states[7].prev_gyro_timestamp = 99;

    mm64::input_hotplug::handle_controller_added(controller_states, cur_controllers, second, 7);

    CHECK(g_closed_controllers.size() == 1);
    CHECK(g_closed_controllers[0] == first);
    CHECK(controller_states.size() == 1);
    CHECK(controller_states[7].controller == second);
    CHECK(controller_states[7].latest_accelerometer == g_zero_accelerometer);
    CHECK(controller_states[7].prev_gyro_timestamp == 0);
    CHECK(cur_controllers.size() == 1);
    CHECK(cur_controllers[0] == second);
}

static void test_remove_controller_updates_connected_state() {
    reset_stub_state();

    mm64::input_hotplug::ControllerStateMap controller_states{};
    std::vector<SDL_GameController*> cur_controllers{};

    SDL_GameController* first = fake_controller(0x1001);
    SDL_GameController* second = fake_controller(0x1002);

    mm64::input_hotplug::handle_controller_added(controller_states, cur_controllers, first, 7);
    mm64::input_hotplug::handle_controller_added(controller_states, cur_controllers, second, 8);

    mm64::input_hotplug::handle_controller_removed(controller_states, cur_controllers, 7);

    CHECK(g_closed_controllers.size() == 1);
    CHECK(g_closed_controllers[0] == first);
    CHECK(controller_states.size() == 1);
    CHECK(cur_controllers.size() == 1);
    CHECK(cur_controllers[0] == second);
    CHECK(mm64::input_hotplug::has_connected_controller(cur_controllers, controller_states));

    mm64::input_hotplug::handle_controller_removed(controller_states, cur_controllers, 8);

    CHECK(g_closed_controllers.size() == 2);
    CHECK(g_closed_controllers[1] == second);
    CHECK(controller_states.empty());
    CHECK(cur_controllers.empty());
    CHECK(!mm64::input_hotplug::has_connected_controller(cur_controllers, controller_states));
}

static void test_removing_unknown_instance_is_a_noop() {
    reset_stub_state();

    mm64::input_hotplug::ControllerStateMap controller_states{};
    std::vector<SDL_GameController*> cur_controllers{};

    SDL_GameController* controller = fake_controller(0x1001);
    mm64::input_hotplug::handle_controller_added(controller_states, cur_controllers, controller, 7);
    mm64::input_hotplug::handle_controller_removed(controller_states, cur_controllers, 42);

    CHECK(g_closed_controllers.empty());
    CHECK(controller_states.size() == 1);
    CHECK(cur_controllers.size() == 1);
    CHECK(cur_controllers[0] == controller);
}

static void test_rebuild_restores_connected_handles_from_controller_state() {
    reset_stub_state();

    mm64::input_hotplug::ControllerStateMap controller_states{};
    std::vector<SDL_GameController*> cur_controllers{};

    controller_states[7].controller = fake_controller(0x1001);
    controller_states[8].controller = nullptr;

    mm64::input_hotplug::rebuild_connected_controllers(cur_controllers, controller_states);

    CHECK(cur_controllers.size() == 1);
    CHECK(cur_controllers[0] == controller_states[7].controller);
    CHECK(mm64::input_hotplug::has_connected_controller(cur_controllers, controller_states));

    cur_controllers.clear();
    CHECK(mm64::input_hotplug::has_connected_controller(cur_controllers, controller_states));
}

int main() {
    test_add_controller_marks_connected_and_tracks_handle();
    test_readding_same_instance_replaces_and_closes_old_handle();
    test_remove_controller_updates_connected_state();
    test_removing_unknown_instance_is_a_noop();
    test_rebuild_restores_connected_handles_from_controller_state();
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
