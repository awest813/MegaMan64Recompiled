#ifndef __MM64_PATHS_HPP__
#define __MM64_PATHS_HPP__

#include <filesystem>
#include <string>
#include <string_view>

namespace zelda64::paths {
    inline constexpr std::u8string_view general_config_filename = u8"general.json";
    inline constexpr std::u8string_view graphics_config_filename = u8"graphics.json";
    inline constexpr std::u8string_view controls_config_filename = u8"controls.json";
    inline constexpr std::u8string_view sound_config_filename = u8"sound.json";
    inline constexpr std::u8string_view saves_directory = u8"saves";

    inline bool has_portable_mode_marker(const std::filesystem::path& dir) {
        return std::filesystem::exists(dir / "portable.txt");
    }

    inline std::filesystem::path config_file_path(const std::filesystem::path& app_dir, std::u8string_view filename) {
        return app_dir / std::filesystem::path{ filename };
    }

    inline std::filesystem::path save_file_path(
        const std::filesystem::path& app_dir,
        std::u8string_view name,
        std::u8string_view subfolder = {}
    ) {
        std::filesystem::path save_dir = app_dir / std::filesystem::path{ saves_directory };
        if (!subfolder.empty()) {
            save_dir /= std::filesystem::path{ subfolder };
        }

        std::u8string filename{ name };
        filename += u8".bin";
        return save_dir / std::filesystem::path{ filename };
    }
}

#endif
