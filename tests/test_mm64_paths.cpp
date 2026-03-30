#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "mm64_paths.hpp"

static int failures = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            failures++; \
            return; \
        } \
    } while (0)

static std::filesystem::path make_temp_dir(const char* name) {
    std::filesystem::path temp = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
    std::filesystem::create_directories(temp, ec);
    return temp;
}

static void test_portable_mode_marker() {
    const std::filesystem::path temp = make_temp_dir("mm64_test_paths_portable");
    CHECK(!zelda64::paths::has_portable_mode_marker(temp));

    std::ofstream portable_file(temp / "portable.txt");
    portable_file << "portable";
    portable_file.close();

    CHECK(zelda64::paths::has_portable_mode_marker(temp));

    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
}

static void test_config_file_paths() {
    const std::filesystem::path app_dir = std::filesystem::path("D:/tmp/mm64");

    CHECK(zelda64::paths::config_file_path(app_dir, zelda64::paths::general_config_filename) == app_dir / "general.json");
    CHECK(zelda64::paths::config_file_path(app_dir, zelda64::paths::graphics_config_filename) == app_dir / "graphics.json");
    CHECK(zelda64::paths::config_file_path(app_dir, zelda64::paths::controls_config_filename) == app_dir / "controls.json");
    CHECK(zelda64::paths::config_file_path(app_dir, zelda64::paths::sound_config_filename) == app_dir / "sound.json");
}

static void test_save_file_paths() {
    const std::filesystem::path app_dir = std::filesystem::path("D:/tmp/mm64");

    CHECK(zelda64::paths::save_file_path(app_dir, u8"megaman.n64.us.1.0") == app_dir / "saves" / "megaman.n64.us.1.0.bin");
    CHECK(zelda64::paths::save_file_path(app_dir, u8"profile1", u8"mods") == app_dir / "saves" / "mods" / "profile1.bin");
}

int main() {
    test_portable_mode_marker();
    test_config_file_paths();
    test_save_file_paths();
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
