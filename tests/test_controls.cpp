#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "recomp_input.h"

static int failures = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            failures++; \
            return; \
        } \
    } while (0)

namespace {

bool g_game_input_disabled = false;
std::vector<recomp::InputField> g_pressed_fields{};
std::vector<std::pair<recomp::InputField, float>> g_analog_fields{};

bool field_matches(const recomp::InputField& lhs, const recomp::InputField& rhs) {
    return lhs.input_type == rhs.input_type && lhs.input_id == rhs.input_id;
}

bool is_pressed(const recomp::InputField& field) {
    return std::ranges::any_of(g_pressed_fields, [&](const recomp::InputField& pressed) {
        return field_matches(field, pressed);
    });
}

float analog_value(const recomp::InputField& field) {
    const auto match = std::ranges::find_if(g_analog_fields, [&](const auto& analog_entry) {
        return field_matches(field, analog_entry.first);
    });
    return match == g_analog_fields.end() ? 0.0f : match->second;
}

void reset_stub_state() {
    g_game_input_disabled = false;
    g_pressed_fields.clear();
    g_analog_fields.clear();
}

bool nearly_equal(float lhs, float rhs, float epsilon = 0.0001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

} // namespace

namespace recomp {

float get_input_analog(const InputField& field) {
    return analog_value(field);
}

float get_input_analog(const std::span<const InputField> fields) {
    float total = 0.0f;
    for (const InputField& field : fields) {
        total += get_input_analog(field);
    }
    return std::clamp(total, 0.0f, 1.0f);
}

bool get_input_digital(const InputField& field) {
    return is_pressed(field);
}

bool get_input_digital(const std::span<const InputField> fields) {
    return std::ranges::any_of(fields, [](const InputField& field) {
        return get_input_digital(field);
    });
}

int get_joystick_deadzone() {
    return 0;
}

void apply_joystick_deadzone(float x_in, float y_in, float* x_out, float* y_out) {
    *x_out = x_in;
    *y_out = y_in;
}

bool game_input_disabled() {
    return g_game_input_disabled;
}

static InputField s_dummy_binding{};

InputField& get_input_binding(int port_index, GameInput input, size_t binding_index, InputDevice device) {
    (void)port_index; (void)input; (void)binding_index; (void)device;
    return s_dummy_binding;
}

void set_input_binding(int port_index, GameInput input, size_t binding_index, InputDevice device, InputField value) {
    (void)port_index; (void)input; (void)binding_index; (void)device; (void)value;
}

} // namespace recomp

static void test_input_enum_name_round_trip() {
    for (size_t i = 0; i < recomp::get_num_inputs(); i++) {
        const recomp::GameInput input = static_cast<recomp::GameInput>(i);
        CHECK(recomp::get_input_from_enum_name(recomp::get_input_enum_name(input)) == input);
    }

    CHECK(recomp::get_input_from_enum_name("NOT_A_REAL_INPUT") == recomp::GameInput::COUNT);
}

static void test_bindings_are_isolated_per_device_and_slot() {
    constexpr int port = 0;
    const recomp::InputField keyboard_primary{10, 100};
    const recomp::InputField keyboard_secondary{11, 101};
    const recomp::InputField controller_primary{20, 200};

    recomp::set_input_binding(port, recomp::GameInput::A, 0, recomp::InputDevice::Keyboard, keyboard_primary);
    recomp::set_input_binding(port, recomp::GameInput::A, 1, recomp::InputDevice::Keyboard, keyboard_secondary);
    recomp::set_input_binding(port, recomp::GameInput::A, 0, recomp::InputDevice::Controller, controller_primary);

    CHECK(recomp::get_input_binding(port, recomp::GameInput::A, 0, recomp::InputDevice::Keyboard) == keyboard_primary);
    CHECK(recomp::get_input_binding(port, recomp::GameInput::A, 1, recomp::InputDevice::Keyboard) == keyboard_secondary);
    CHECK(recomp::get_input_binding(port, recomp::GameInput::A, 0, recomp::InputDevice::Controller) == controller_primary);
    CHECK(recomp::get_input_binding(port, recomp::GameInput::A, 1, recomp::InputDevice::Controller) == recomp::InputField{});
}

static void test_invalid_binding_index_uses_fresh_dummy_and_preserves_real_bindings() {
    constexpr int port = 0;
    const recomp::InputField valid_binding{30, 300};
    recomp::set_input_binding(port, recomp::GameInput::B, 0, recomp::InputDevice::Keyboard, valid_binding);

    recomp::InputField& invalid_binding = recomp::get_input_binding(port, recomp::GameInput::B, 99, recomp::InputDevice::Keyboard);
    invalid_binding = {99, 999};

    CHECK(recomp::get_input_binding(port, recomp::GameInput::B, 0, recomp::InputDevice::Keyboard) == valid_binding);

    const recomp::InputField other_invalid_binding =
        recomp::get_input_binding(port, recomp::GameInput::START, 42, recomp::InputDevice::Controller);
    CHECK(other_invalid_binding == recomp::InputField{});

    recomp::set_input_binding(port, recomp::GameInput::B, 99, recomp::InputDevice::Keyboard, {77, 777});
    CHECK(recomp::get_input_binding(port, recomp::GameInput::B, 0, recomp::InputDevice::Keyboard) == valid_binding);
}

static void test_get_n64_input_merges_sources_and_clamps_axes() {
    constexpr int port = 0;
    reset_stub_state();

    const recomp::InputField keyboard_a{1, 1};
    const recomp::InputField controller_b{2, 2};
    const recomp::InputField keyboard_x_pos{3, 3};
    const recomp::InputField keyboard_x_neg{3, 4};
    const recomp::InputField keyboard_y_pos{3, 5};
    const recomp::InputField controller_y_pos{4, 6};
    const recomp::InputField controller_y_neg{4, 7};

    recomp::set_input_binding(port, recomp::GameInput::A, 0, recomp::InputDevice::Keyboard, keyboard_a);
    recomp::set_input_binding(port, recomp::GameInput::B, 0, recomp::InputDevice::Controller, controller_b);
    recomp::set_input_binding(port, recomp::GameInput::X_AXIS_POS, 0, recomp::InputDevice::Keyboard, keyboard_x_pos);
    recomp::set_input_binding(port, recomp::GameInput::X_AXIS_NEG, 0, recomp::InputDevice::Keyboard, keyboard_x_neg);
    recomp::set_input_binding(port, recomp::GameInput::Y_AXIS_POS, 0, recomp::InputDevice::Keyboard, keyboard_y_pos);
    recomp::set_input_binding(port, recomp::GameInput::Y_AXIS_POS, 0, recomp::InputDevice::Controller, controller_y_pos);
    recomp::set_input_binding(port, recomp::GameInput::Y_AXIS_NEG, 0, recomp::InputDevice::Controller, controller_y_neg);

    g_pressed_fields = {keyboard_a, controller_b};
    g_analog_fields = {
        {keyboard_x_pos, 0.80f},
        {keyboard_x_neg, 0.10f},
        {keyboard_y_pos, 0.75f},
        {controller_y_pos, 0.90f},
        {controller_y_neg, 0.05f},
    };

    uint16_t buttons = 0;
    float x = 0.0f;
    float y = 0.0f;
    CHECK(recomp::get_n64_input(0, &buttons, &x, &y));
    CHECK(buttons == static_cast<uint16_t>(0x8000u | 0x4000u));
    CHECK(nearly_equal(x, 0.70f));
    CHECK(nearly_equal(y, 1.0f));
}

static void test_get_n64_input_rejects_non_primary_controller() {
    uint16_t buttons = 123;
    float x = 4.0f;
    float y = 5.0f;

    CHECK(!recomp::get_n64_input(1, &buttons, &x, &y));
    CHECK(buttons == 123);
    CHECK(nearly_equal(x, 4.0f));
    CHECK(nearly_equal(y, 5.0f));
}

int main() {
    test_input_enum_name_round_trip();
    test_bindings_are_isolated_per_device_and_slot();
    test_invalid_binding_index_uses_fresh_dummy_and_preserves_real_bindings();
    test_get_n64_input_merges_sources_and_clamps_axes();
    test_get_n64_input_rejects_non_primary_controller();
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
