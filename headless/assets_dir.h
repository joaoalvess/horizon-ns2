// SPDX-License-Identifier: GPL-3.0-or-later
// The game files folder: keys/, firmware/ and roms/ in the folder chosen in Settings > Game
// files (/data/eglantineps5 by default).
// Resolved once per process; a new choice applies when EglantinePS5 is reopened. Where the rest
// lives: storage_paths.h; what is saved: settings_store.h.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "settings_store.h"
#include "storage_paths.h"

namespace Eden {
// The saved folder, else /data/eglantineps5, except that an install from before the setting
// keeps the app folder's assets/ while /data/eglantineps5 has no keys (main.cpp moves those
// files to /data/eglantineps5 when it can). Without filesystem access nothing can be read.
inline std::string ResolveAssetsDir() {
    const std::string legacy = AppFile("assets");
    if (!FilesystemAccess()) return kDefaultAssetsDir;
    if (std::string saved = LoadSavedAssetsDir(); !saved.empty()) return saved;
    const std::string keys = "/keys/prod.keys";
    if (!FileExists(kDefaultAssetsDir + keys) && FileExists(legacy + keys)) return legacy;
    return kDefaultAssetsDir;
}

// The game files folder in use by this process.
inline const std::string& AssetsDir() {
    static const std::string directory = ResolveAssetsDir();
    return directory;
}
inline std::string AssetsPath(std::string_view child) { return AssetsDir() + "/" + std::string(child); }

inline std::string PlatformFolder(std::uint64_t title_id) { return (title_id >> 56) == 0x04 ? "switch2" : "switch1"; }

inline std::string ModsFolder(std::uint64_t title_id) {
    const std::string platform = PlatformFolder(title_id);
    return DirectoryExists(AssetsPath(platform)) ? platform + "/mods" : std::string{"mods"};
}

inline std::string ModsRoot(std::uint64_t title_id) { return AssetsPath(ModsFolder(title_id)); }

inline std::vector<std::string> RomsDirs() {
    std::vector<std::string> dirs;
    for (const char* platform : {"switch1/roms", "switch2/roms"})
        if (DirectoryExists(AssetsPath(platform))) dirs.push_back(AssetsPath(platform));
    const std::string fallback = std::string{kDefaultAssetsDir} + "/roms";
    if (AssetsDir() != kDefaultAssetsDir && DirectoryExists(fallback)) dirs.push_back(fallback);
    return dirs;
}

inline std::vector<std::string> UpdatesDirs() {
    std::vector<std::string> dirs{AssetsPath("updates")};
    for (const char* platform : {"switch1/updates", "switch2/updates"})
        if (DirectoryExists(AssetsPath(platform))) dirs.push_back(AssetsPath(platform));
    return dirs;
}
} // namespace Eden
