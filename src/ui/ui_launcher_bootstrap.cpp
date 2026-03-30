#include "launcher_bootstrap.hpp"

#include <array>

namespace {
    constexpr std::array<const char*, 4> preferred_rom_filenames = {
        "megaman64.us.z64",
        "Mega Man 64 (USA).z64",
        "megaman.n64.us.1.0.z64",
        "Mega Man 64.z64",
    };
}

bool recompui::launcher::is_rom_candidate_path(const std::filesystem::path& path) {
    const std::filesystem::path extension = path.extension();
    return extension == ".z64" || extension == ".n64" || extension == ".v64";
}

bool recompui::launcher::try_cache_local_rom(const std::filesystem::path& search_dir, const rom_selection_callback_t& try_select_rom) {
    if (!std::filesystem::exists(search_dir) || !std::filesystem::is_directory(search_dir)) {
        return false;
    }

    for (const char* candidate_name : preferred_rom_filenames) {
        const std::filesystem::path candidate = search_dir / candidate_name;
        if (std::filesystem::is_regular_file(candidate) && try_select_rom(candidate)) {
            return true;
        }
    }

    for (const auto& entry : std::filesystem::directory_iterator(search_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        if (!is_rom_candidate_path(entry.path())) {
            continue;
        }

        if (try_select_rom(entry.path())) {
            return true;
        }
    }

    return false;
}

bool recompui::launcher::should_autostart_game(bool rom_valid, bool game_started) {
    return rom_valid && !game_started;
}
