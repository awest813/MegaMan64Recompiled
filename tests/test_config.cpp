#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "recomp_input.h"
#include "zelda_config.h"
#include "zelda_sound.h"
#include "mm64_paths.hpp"
#include "ultramodern/config.hpp"

using json = nlohmann::json;

bool save_general_config(const std::filesystem::path& path);
bool load_general_config(const std::filesystem::path& path);
bool save_graphics_config(const std::filesystem::path& path);
bool load_graphics_config(const std::filesystem::path& path);
bool save_controls_config(const std::filesystem::path& path);
bool load_controls_config(const std::filesystem::path& path);
bool save_sound_config(const std::filesystem::path& path);
bool load_sound_config(const std::filesystem::path& path);

static int failures = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            failures++; \
            return; \
        } \
    } while (0)

namespace {

using input_mapping = std::array<recomp::InputField, recomp::bindings_per_input>;
using input_mapping_array = std::array<input_mapping, static_cast<size_t>(recomp::GameInput::COUNT)>;

struct GeneralState {
    zelda64::TargetingMode targeting_mode = zelda64::TargetingMode::Switch;
    recomp::BackgroundInputMode background_input_mode = recomp::BackgroundInputMode::On;
    int rumble_strength = 25;
    int gyro_sensitivity = 50;
    int mouse_sensitivity = 0;
    int joystick_deadzone = 5;
    zelda64::AutosaveMode autosave_mode = zelda64::AutosaveMode::On;
    zelda64::CameraInvertMode camera_invert_mode = zelda64::CameraInvertMode::InvertY;
    zelda64::AnalogCamMode analog_cam_mode = zelda64::AnalogCamMode::Off;
    zelda64::CameraInvertMode analog_camera_invert_mode = zelda64::CameraInvertMode::InvertNone;
    bool debug_mode = false;
};

struct SoundState {
    int main_volume = 80;
    int bgm_volume = 70;
    bool low_health_beeps = false;
};

GeneralState g_general_state{};
SoundState g_sound_state{};
ultramodern::renderer::GraphicsConfig g_graphics_config{};
input_mapping_array g_keyboard_bindings{};
input_mapping_array g_controller_bindings{};
std::vector<std::string> g_message_boxes{};

} // anonymous namespace

namespace recomp {

static InputField s_dummy_binding{};

InputField& get_input_binding(int port_index, GameInput input, size_t binding_index, InputDevice device) {
    (void)port_index; (void)input; (void)binding_index; (void)device;
    return s_dummy_binding;
}

void set_input_binding(int port_index, GameInput input, size_t binding_index, InputDevice device, InputField value) {
    (void)port_index; (void)input; (void)binding_index; (void)device; (void)value;
}

const DefaultN64Mappings classic_n64_keyboard_mappings{};
const DefaultN64Mappings classic_n64_controller_mappings{};

} // namespace recomp

namespace {

std::filesystem::path backup_path_for(const std::filesystem::path& path) {
    return std::filesystem::path{ path.string() + ".bak" };
}

std::filesystem::path make_temp_dir(const char* name) {
    const std::filesystem::path temp = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
    std::filesystem::create_directories(temp, ec);
    return temp;
}

void write_text(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

void write_json(const std::filesystem::path& path, const json& value) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc);
    output << value.dump(4);
}

recomp::InputField make_field(uint32_t type, int32_t id) {
    return recomp::InputField{ .input_type = type, .input_id = id };
}

recomp::DefaultN64Mappings make_default_mappings(uint32_t type_base, int32_t id_base) {
    return recomp::DefaultN64Mappings{
        .a = { make_field(type_base, id_base + 1) },
        .b = { make_field(type_base, id_base + 2) },
        .l = { make_field(type_base, id_base + 3) },
        .r = { make_field(type_base, id_base + 4) },
        .z = { make_field(type_base, id_base + 5) },
        .start = { make_field(type_base, id_base + 6) },
        .c_left = { make_field(type_base, id_base + 7) },
        .c_right = { make_field(type_base, id_base + 8) },
        .c_up = { make_field(type_base, id_base + 9) },
        .c_down = { make_field(type_base, id_base + 10) },
        .dpad_left = { make_field(type_base, id_base + 11) },
        .dpad_right = { make_field(type_base, id_base + 12) },
        .dpad_up = { make_field(type_base, id_base + 13) },
        .dpad_down = { make_field(type_base, id_base + 14) },
        .analog_left = { make_field(type_base, id_base + 15) },
        .analog_right = { make_field(type_base, id_base + 16) },
        .analog_up = { make_field(type_base, id_base + 17) },
        .analog_down = { make_field(type_base, id_base + 18) },
        .toggle_menu = { make_field(type_base, id_base + 19) },
        .accept_menu = { make_field(type_base, id_base + 20) },
        .apply_menu = { make_field(type_base, id_base + 21) },
    };
}

ultramodern::renderer::GraphicsConfig make_graphics_config(
    bool developer_mode,
    ultramodern::renderer::Resolution resolution,
    ultramodern::renderer::WindowMode window_mode,
    ultramodern::renderer::HUDRatioMode hud_ratio_mode,
    ultramodern::renderer::GraphicsApi graphics_api,
    ultramodern::renderer::AspectRatio aspect_ratio,
    ultramodern::renderer::Antialiasing antialiasing,
    ultramodern::renderer::RefreshRate refresh_rate,
    ultramodern::renderer::HighPrecisionFramebuffer high_precision_framebuffer,
    int manual_refresh_rate,
    int downsample_option
) {
    ultramodern::renderer::GraphicsConfig config{};
    config.developer_mode = developer_mode;
    config.res_option = resolution;
    config.wm_option = window_mode;
    config.hr_option = hud_ratio_mode;
    config.api_option = graphics_api;
    config.ar_option = aspect_ratio;
    config.msaa_option = antialiasing;
    config.rr_option = refresh_rate;
    config.hpfb_option = high_precision_framebuffer;
    config.rr_manual_value = manual_refresh_rate;
    config.ds_option = downsample_option;
    return config;
}

void reset_stub_state() {
    g_general_state = {};
    g_sound_state = {};
    g_graphics_config = make_graphics_config(
        false,
        ultramodern::renderer::Resolution::Auto,
        ultramodern::renderer::WindowMode::Windowed,
        ultramodern::renderer::HUDRatioMode::Clamp16x9,
        ultramodern::renderer::GraphicsApi::Auto,
        ultramodern::renderer::AspectRatio::Expand,
        ultramodern::renderer::Antialiasing::MSAA2X,
        ultramodern::renderer::RefreshRate::Display,
        ultramodern::renderer::HighPrecisionFramebuffer::Auto,
        60,
        1
    );
    g_keyboard_bindings = {};
    g_controller_bindings = {};
    g_message_boxes.clear();
}

} // namespace

namespace recomp {

const DefaultN64Mappings default_n64_keyboard_mappings = make_default_mappings(1, 1000);
const DefaultN64Mappings default_n64_controller_mappings = make_default_mappings(2, 2000);

std::ifstream open_input_backup_file(const std::filesystem::path& filepath, std::ios_base::openmode mode) {
    return std::ifstream(backup_path_for(filepath), mode);
}

std::ofstream open_output_file_with_backup(const std::filesystem::path& filepath, std::ios_base::openmode mode) {
    std::filesystem::create_directories(filepath.parent_path());
    return std::ofstream(filepath, mode | std::ios::trunc);
}

bool finalize_output_file_with_backup(const std::filesystem::path& filepath) {
    std::error_code ec;
    std::filesystem::copy_file(filepath, backup_path_for(filepath),
        std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
}

size_t get_num_inputs() {
    return static_cast<size_t>(GameInput::COUNT);
}

const std::string& get_input_enum_name(GameInput input) {
    #define DEFINE_INPUT(name, value, readable) #name,
    static const std::vector<std::string> input_enum_names = {
        DEFINE_ALL_INPUTS()
    };
    #undef DEFINE_INPUT
    return input_enum_names.at(static_cast<size_t>(input));
}

InputField& get_input_binding(GameInput input, size_t binding_index, InputDevice device) {
    input_mapping_array& device_mappings = (device == InputDevice::Controller) ? g_controller_bindings : g_keyboard_bindings;
    input_mapping& cur_input_mapping = device_mappings.at(static_cast<size_t>(input));

    if (binding_index < cur_input_mapping.size()) {
        return cur_input_mapping[binding_index];
    }

    static InputField dummy{};
    dummy = {};
    return dummy;
}

void set_input_binding(GameInput input, size_t binding_index, InputDevice device, InputField value) {
    input_mapping_array& device_mappings = (device == InputDevice::Controller) ? g_controller_bindings : g_keyboard_bindings;
    input_mapping& cur_input_mapping = device_mappings.at(static_cast<size_t>(input));

    if (binding_index < cur_input_mapping.size()) {
        cur_input_mapping[binding_index] = value;
    }
}

BackgroundInputMode get_background_input_mode() {
    return g_general_state.background_input_mode;
}

void set_background_input_mode(BackgroundInputMode mode) {
    g_general_state.background_input_mode = mode;
}

int get_rumble_strength() {
    return g_general_state.rumble_strength;
}

void set_rumble_strength(int strength) {
    g_general_state.rumble_strength = strength;
}

int get_gyro_sensitivity() {
    return g_general_state.gyro_sensitivity;
}

int get_mouse_sensitivity() {
    return g_general_state.mouse_sensitivity;
}

int get_joystick_deadzone() {
    return g_general_state.joystick_deadzone;
}

void set_gyro_sensitivity(int strength) {
    g_general_state.gyro_sensitivity = strength;
}

void set_mouse_sensitivity(int strength) {
    g_general_state.mouse_sensitivity = strength;
}

void set_joystick_deadzone(int strength) {
    g_general_state.joystick_deadzone = strength;
}

} // namespace recomp

namespace ultramodern::renderer {

const GraphicsConfig& get_graphics_config() {
    return g_graphics_config;
}

void set_graphics_config(const GraphicsConfig& new_config) {
    g_graphics_config = new_config;
}

} // namespace ultramodern::renderer

namespace zelda64 {

void show_error_message_box(const char* title, const char* message) {
    g_message_boxes.push_back(std::string(title) + ": " + message);
}

TargetingMode get_targeting_mode() {
    return g_general_state.targeting_mode;
}

void set_targeting_mode(TargetingMode mode) {
    g_general_state.targeting_mode = mode;
}

AutosaveMode get_autosave_mode() {
    return g_general_state.autosave_mode;
}

void set_autosave_mode(AutosaveMode mode) {
    g_general_state.autosave_mode = mode;
}

CameraInvertMode get_camera_invert_mode() {
    return g_general_state.camera_invert_mode;
}

void set_camera_invert_mode(CameraInvertMode mode) {
    g_general_state.camera_invert_mode = mode;
}

CameraInvertMode get_analog_camera_invert_mode() {
    return g_general_state.analog_camera_invert_mode;
}

void set_analog_camera_invert_mode(CameraInvertMode mode) {
    g_general_state.analog_camera_invert_mode = mode;
}

AnalogCamMode get_analog_cam_mode() {
    return g_general_state.analog_cam_mode;
}

void set_analog_cam_mode(AnalogCamMode mode) {
    g_general_state.analog_cam_mode = mode;
}

bool get_debug_mode_enabled() {
    return g_general_state.debug_mode;
}

void set_debug_mode_enabled(bool enabled) {
    g_general_state.debug_mode = enabled;
}

void reset_sound_settings() {
    g_sound_state = {};
}

void set_main_volume(int volume) {
    g_sound_state.main_volume = volume;
}

int get_main_volume() {
    return g_sound_state.main_volume;
}

void set_bgm_volume(int volume) {
    g_sound_state.bgm_volume = volume;
}

int get_bgm_volume() {
    return g_sound_state.bgm_volume;
}

void set_low_health_beeps_enabled(bool enabled) {
    g_sound_state.low_health_beeps = enabled;
}

bool get_low_health_beeps_enabled() {
    return g_sound_state.low_health_beeps;
}

} // namespace zelda64

static void test_general_config_round_trip() {
    reset_stub_state();
    const std::filesystem::path temp = make_temp_dir("mm64_test_config_general_round_trip");
    const std::filesystem::path path = temp / "general.json";

    g_general_state = GeneralState{
        .targeting_mode = zelda64::TargetingMode::Hold,
        .background_input_mode = recomp::BackgroundInputMode::Off,
        .rumble_strength = 77,
        .gyro_sensitivity = 66,
        .mouse_sensitivity = 23,
        .joystick_deadzone = 14,
        .autosave_mode = zelda64::AutosaveMode::Off,
        .camera_invert_mode = zelda64::CameraInvertMode::InvertBoth,
        .analog_cam_mode = zelda64::AnalogCamMode::On,
        .analog_camera_invert_mode = zelda64::CameraInvertMode::InvertX,
        .debug_mode = true,
    };

    CHECK(save_general_config(path));

    g_general_state = {};
    CHECK(load_general_config(path));
    CHECK(g_general_state.targeting_mode == zelda64::TargetingMode::Hold);
    CHECK(g_general_state.background_input_mode == recomp::BackgroundInputMode::Off);
    CHECK(g_general_state.rumble_strength == 77);
    CHECK(g_general_state.gyro_sensitivity == 66);
    CHECK(g_general_state.mouse_sensitivity == 23);
    CHECK(g_general_state.joystick_deadzone == 14);
    CHECK(g_general_state.autosave_mode == zelda64::AutosaveMode::Off);
    CHECK(g_general_state.camera_invert_mode == zelda64::CameraInvertMode::InvertBoth);
    CHECK(g_general_state.analog_cam_mode == zelda64::AnalogCamMode::On);
    CHECK(g_general_state.analog_camera_invert_mode == zelda64::CameraInvertMode::InvertX);
    CHECK(g_general_state.debug_mode);
}

static void test_general_config_missing_keys_use_defaults() {
    reset_stub_state();
    const std::filesystem::path temp = make_temp_dir("mm64_test_config_general_missing");
    const std::filesystem::path path = temp / "general.json";

    g_general_state = GeneralState{
        .targeting_mode = zelda64::TargetingMode::Hold,
        .background_input_mode = recomp::BackgroundInputMode::Off,
        .rumble_strength = 1,
        .gyro_sensitivity = 2,
        .mouse_sensitivity = 3,
        .joystick_deadzone = 4,
        .autosave_mode = zelda64::AutosaveMode::Off,
        .camera_invert_mode = zelda64::CameraInvertMode::InvertBoth,
        .analog_cam_mode = zelda64::AnalogCamMode::On,
        .analog_camera_invert_mode = zelda64::CameraInvertMode::InvertX,
        .debug_mode = true,
    };

    write_json(path, json{
        {"mouse_sensitivity", 17},
        {"debug_mode", true}
    });

    CHECK(load_general_config(path));
    CHECK(g_general_state.targeting_mode == zelda64::TargetingMode::Switch);
    CHECK(g_general_state.background_input_mode == recomp::BackgroundInputMode::On);
    CHECK(g_general_state.rumble_strength == 25);
    CHECK(g_general_state.gyro_sensitivity == 50);
    CHECK(g_general_state.mouse_sensitivity == 17);
    CHECK(g_general_state.joystick_deadzone == 5);
    CHECK(g_general_state.autosave_mode == zelda64::AutosaveMode::On);
    CHECK(g_general_state.camera_invert_mode == zelda64::CameraInvertMode::InvertY);
    CHECK(g_general_state.analog_cam_mode == zelda64::AnalogCamMode::Off);
    CHECK(g_general_state.analog_camera_invert_mode == zelda64::CameraInvertMode::InvertNone);
    CHECK(g_general_state.debug_mode);
}

static void test_general_config_corrupt_json_uses_backup() {
    reset_stub_state();
    const std::filesystem::path temp = make_temp_dir("mm64_test_config_general_backup");
    const std::filesystem::path path = temp / "general.json";

    write_text(path, "{ not valid json");
    write_json(backup_path_for(path), json{
        {"targeting_mode", "Hold"},
        {"gyro_sensitivity", 91}
    });

    CHECK(load_general_config(path));
    CHECK(g_general_state.targeting_mode == zelda64::TargetingMode::Hold);
    CHECK(g_general_state.gyro_sensitivity == 91);
}

static void test_graphics_config_round_trip() {
    reset_stub_state();
    const std::filesystem::path temp = make_temp_dir("mm64_test_config_graphics_round_trip");
    const std::filesystem::path path = temp / "graphics.json";

    g_graphics_config = make_graphics_config(
        true,
        ultramodern::renderer::Resolution::Original2x,
        ultramodern::renderer::WindowMode::Fullscreen,
        ultramodern::renderer::HUDRatioMode::Full,
        ultramodern::renderer::GraphicsApi::Vulkan,
        ultramodern::renderer::AspectRatio::Manual,
        ultramodern::renderer::Antialiasing::MSAA8X,
        ultramodern::renderer::RefreshRate::Manual,
        ultramodern::renderer::HighPrecisionFramebuffer::On,
        144,
        4
    );

    CHECK(save_graphics_config(path));

    g_graphics_config = {};
    CHECK(load_graphics_config(path));
    CHECK(g_graphics_config.developer_mode);
    CHECK(g_graphics_config.res_option == ultramodern::renderer::Resolution::Original2x);
    CHECK(g_graphics_config.wm_option == ultramodern::renderer::WindowMode::Fullscreen);
    CHECK(g_graphics_config.hr_option == ultramodern::renderer::HUDRatioMode::Full);
    CHECK(g_graphics_config.api_option == ultramodern::renderer::GraphicsApi::Vulkan);
    CHECK(g_graphics_config.ar_option == ultramodern::renderer::AspectRatio::Manual);
    CHECK(g_graphics_config.msaa_option == ultramodern::renderer::Antialiasing::MSAA8X);
    CHECK(g_graphics_config.rr_option == ultramodern::renderer::RefreshRate::Manual);
    CHECK(g_graphics_config.hpfb_option == ultramodern::renderer::HighPrecisionFramebuffer::On);
    CHECK(g_graphics_config.rr_manual_value == 144);
    CHECK(g_graphics_config.ds_option == 4);
}

static void test_graphics_config_missing_keys_use_defaults() {
    reset_stub_state();
    const std::filesystem::path temp = make_temp_dir("mm64_test_config_graphics_missing");
    const std::filesystem::path path = temp / "graphics.json";

    g_graphics_config = make_graphics_config(
        false,
        ultramodern::renderer::Resolution::Original,
        ultramodern::renderer::WindowMode::Fullscreen,
        ultramodern::renderer::HUDRatioMode::Full,
        ultramodern::renderer::GraphicsApi::D3D12,
        ultramodern::renderer::AspectRatio::Manual,
        ultramodern::renderer::Antialiasing::MSAA8X,
        ultramodern::renderer::RefreshRate::Manual,
        ultramodern::renderer::HighPrecisionFramebuffer::On,
        240,
        8
    );

    write_json(path, json{
        {"developer_mode", true},
        {"ds_option", 3}
    });

    CHECK(load_graphics_config(path));
    CHECK(g_graphics_config.developer_mode);
    CHECK(g_graphics_config.res_option == ultramodern::renderer::Resolution::Auto);
    CHECK(g_graphics_config.wm_option == ultramodern::renderer::WindowMode::Windowed);
    CHECK(g_graphics_config.hr_option == ultramodern::renderer::HUDRatioMode::Clamp16x9);
    CHECK(g_graphics_config.api_option == ultramodern::renderer::GraphicsApi::Auto);
    CHECK(g_graphics_config.ar_option == ultramodern::renderer::AspectRatio::Expand);
    CHECK(g_graphics_config.msaa_option == ultramodern::renderer::Antialiasing::MSAA2X);
    CHECK(g_graphics_config.rr_option == ultramodern::renderer::RefreshRate::Display);
    CHECK(g_graphics_config.hpfb_option == ultramodern::renderer::HighPrecisionFramebuffer::Auto);
    CHECK(g_graphics_config.rr_manual_value == 60);
    CHECK(g_graphics_config.ds_option == 3);
}

static void test_graphics_config_corrupt_json_uses_backup() {
    reset_stub_state();
    const std::filesystem::path temp = make_temp_dir("mm64_test_config_graphics_backup");
    const std::filesystem::path path = temp / "graphics.json";

    write_text(path, "{ bad json");
    write_json(backup_path_for(path), json{
        {"wm_option", "Fullscreen"},
        {"rr_manual_value", 72},
        {"rr_option", "Manual"}
    });

    CHECK(load_graphics_config(path));
    CHECK(g_graphics_config.wm_option == ultramodern::renderer::WindowMode::Fullscreen);
    CHECK(g_graphics_config.rr_option == ultramodern::renderer::RefreshRate::Manual);
    CHECK(g_graphics_config.rr_manual_value == 72);
}

static void test_controls_config_round_trip() {
    reset_stub_state();
    const std::filesystem::path temp = make_temp_dir("mm64_test_config_controls_round_trip");
    const std::filesystem::path path = temp / "controls.json";

    recomp::set_input_binding(recomp::GameInput::A, 0, recomp::InputDevice::Keyboard, make_field(10, 100));
    recomp::set_input_binding(recomp::GameInput::A, 1, recomp::InputDevice::Keyboard, make_field(11, 101));
    recomp::set_input_binding(recomp::GameInput::B, 0, recomp::InputDevice::Controller, make_field(20, 200));
    recomp::set_input_binding(recomp::GameInput::START, 1, recomp::InputDevice::Controller, make_field(21, 201));

    CHECK(save_controls_config(path));

    g_keyboard_bindings = {};
    g_controller_bindings = {};

    CHECK(load_controls_config(path));
    CHECK(recomp::get_input_binding(recomp::GameInput::A, 0, recomp::InputDevice::Keyboard) == make_field(10, 100));
    CHECK(recomp::get_input_binding(recomp::GameInput::A, 1, recomp::InputDevice::Keyboard) == make_field(11, 101));
    CHECK(recomp::get_input_binding(recomp::GameInput::B, 0, recomp::InputDevice::Controller) == make_field(20, 200));
    CHECK(recomp::get_input_binding(recomp::GameInput::START, 1, recomp::InputDevice::Controller) == make_field(21, 201));
}

static void test_controls_config_missing_keys_reset_defaults_and_clear_stale_bindings() {
    reset_stub_state();
    const std::filesystem::path temp = make_temp_dir("mm64_test_config_controls_missing");
    const std::filesystem::path path = temp / "controls.json";

    recomp::set_input_binding(recomp::GameInput::A, 0, recomp::InputDevice::Keyboard, make_field(90, 900));
    recomp::set_input_binding(recomp::GameInput::A, 1, recomp::InputDevice::Keyboard, make_field(91, 901));
    recomp::set_input_binding(recomp::GameInput::B, 0, recomp::InputDevice::Keyboard, make_field(92, 902));
    recomp::set_input_binding(recomp::GameInput::B, 1, recomp::InputDevice::Keyboard, make_field(93, 903));

    write_json(path, json{
        {"keyboard", json{
            {"A", json::array({ json{
                {"input_type", 77},
                {"input_id", 707}
            }})}
        }},
        {"controller", json::object()}
    });

    CHECK(load_controls_config(path));
    CHECK(recomp::get_input_binding(recomp::GameInput::A, 0, recomp::InputDevice::Keyboard) == make_field(77, 707));
    CHECK(recomp::get_input_binding(recomp::GameInput::A, 1, recomp::InputDevice::Keyboard) == recomp::InputField{});
    CHECK(recomp::get_input_binding(recomp::GameInput::B, 0, recomp::InputDevice::Keyboard) == recomp::default_n64_keyboard_mappings.b[0]);
    CHECK(recomp::get_input_binding(recomp::GameInput::B, 1, recomp::InputDevice::Keyboard) == recomp::InputField{});
}

static void test_controls_config_corrupt_json_uses_backup() {
    reset_stub_state();
    const std::filesystem::path temp = make_temp_dir("mm64_test_config_controls_backup");
    const std::filesystem::path path = temp / "controls.json";

    write_text(path, "{ nope");
    write_json(backup_path_for(path), json{
        {"keyboard", json{
            {"START", json::array({ json{
                {"input_type", 55},
                {"input_id", 505}
            }})}
        }},
        {"controller", json::object()}
    });

    CHECK(load_controls_config(path));
    CHECK(recomp::get_input_binding(recomp::GameInput::START, 0, recomp::InputDevice::Keyboard) == make_field(55, 505));
}

static void test_sound_config_round_trip() {
    reset_stub_state();
    const std::filesystem::path temp = make_temp_dir("mm64_test_config_sound_round_trip");
    const std::filesystem::path path = temp / "sound.json";

    g_sound_state = SoundState{
        .main_volume = 11,
        .bgm_volume = 22,
        .low_health_beeps = true,
    };

    CHECK(save_sound_config(path));

    g_sound_state = {};
    CHECK(load_sound_config(path));
    CHECK(g_sound_state.main_volume == 11);
    CHECK(g_sound_state.bgm_volume == 22);
    CHECK(g_sound_state.low_health_beeps);
}

static void test_sound_config_missing_keys_use_reset_defaults() {
    reset_stub_state();
    const std::filesystem::path temp = make_temp_dir("mm64_test_config_sound_missing");
    const std::filesystem::path path = temp / "sound.json";

    g_sound_state = SoundState{
        .main_volume = 1,
        .bgm_volume = 2,
        .low_health_beeps = true,
    };

    write_json(path, json{
        {"bgm_volume", 33}
    });

    CHECK(load_sound_config(path));
    CHECK(g_sound_state.main_volume == 80);
    CHECK(g_sound_state.bgm_volume == 33);
    CHECK(!g_sound_state.low_health_beeps);
}

static void test_sound_config_corrupt_json_uses_backup() {
    reset_stub_state();
    const std::filesystem::path temp = make_temp_dir("mm64_test_config_sound_backup");
    const std::filesystem::path path = temp / "sound.json";

    write_text(path, "{ definitely broken");
    write_json(backup_path_for(path), json{
        {"main_volume", 44},
        {"low_health_beeps", true}
    });

    CHECK(load_sound_config(path));
    CHECK(g_sound_state.main_volume == 44);
    CHECK(g_sound_state.bgm_volume == 70);
    CHECK(g_sound_state.low_health_beeps);
}

int main() {
    test_general_config_round_trip();
    test_general_config_missing_keys_use_defaults();
    test_general_config_corrupt_json_uses_backup();

    test_graphics_config_round_trip();
    test_graphics_config_missing_keys_use_defaults();
    test_graphics_config_corrupt_json_uses_backup();

    test_controls_config_round_trip();
    test_controls_config_missing_keys_reset_defaults_and_clear_stale_bindings();
    test_controls_config_corrupt_json_uses_backup();

    test_sound_config_round_trip();
    test_sound_config_missing_keys_use_reset_defaults();
    test_sound_config_corrupt_json_uses_backup();

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
