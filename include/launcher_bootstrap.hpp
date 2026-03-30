#ifndef __LAUNCHER_BOOTSTRAP_HPP__
#define __LAUNCHER_BOOTSTRAP_HPP__

#include <filesystem>
#include <functional>

namespace recompui::launcher {
    using rom_selection_callback_t = std::function<bool(const std::filesystem::path&)>;

    bool is_rom_candidate_path(const std::filesystem::path& path);
    bool try_cache_local_rom(const std::filesystem::path& search_dir, const rom_selection_callback_t& try_select_rom);
    bool should_autostart_game(bool rom_valid, bool game_started);
}

#endif
