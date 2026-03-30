#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

#include "launcher_bootstrap.hpp"

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

static void test_prefers_known_filenames() {
    const std::filesystem::path temp = make_temp_dir("mm64_test_launcher_preferred");
    std::ofstream(temp / "random.z64") << "random";
    std::ofstream(temp / "Mega Man 64 (USA).z64") << "preferred";

    std::vector<std::filesystem::path> attempted_paths;
    bool selected = recompui::launcher::try_cache_local_rom(
        temp,
        [&](const std::filesystem::path& path) {
            attempted_paths.push_back(path.filename());
            return true;
        }
    );

    CHECK(selected);
    CHECK(attempted_paths.size() == 1);
    CHECK(attempted_paths[0] == std::filesystem::path("Mega Man 64 (USA).z64"));

    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
}

static void test_falls_back_to_directory_scan() {
    const std::filesystem::path temp = make_temp_dir("mm64_test_launcher_scan");
    std::ofstream(temp / "notes.txt") << "ignore";
    std::ofstream(temp / "prototype.v64") << "candidate";

    std::vector<std::filesystem::path> attempted_paths;
    bool selected = recompui::launcher::try_cache_local_rom(
        temp,
        [&](const std::filesystem::path& path) {
            attempted_paths.push_back(path.filename());
            return path.filename() == std::filesystem::path("prototype.v64");
        }
    );

    CHECK(selected);
    CHECK(attempted_paths.size() == 1);
    CHECK(attempted_paths[0] == std::filesystem::path("prototype.v64"));

    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
}

static void test_autostart_gate() {
    CHECK(recompui::launcher::should_autostart_game(true, false));
    CHECK(!recompui::launcher::should_autostart_game(false, false));
    CHECK(!recompui::launcher::should_autostart_game(true, true));
}

int main() {
    test_prefers_known_filenames();
    test_falls_back_to_directory_scan();
    test_autostart_gate();
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
