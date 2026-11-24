// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "eglantineps5/pe/core/strings.hpp"
#include "settings_store.h"

namespace Horizon::Patches {

struct Patch {
    const char* id;
    const char* name;
    const char* description;
    std::vector<const char*> keys;
    std::vector<std::uint64_t> recommended;
};

inline const std::vector<Patch>& List() {
    static const std::vector<Patch> patches{
        {"compact_exits", TR("Compact JIT exits"),
         TR("Jumps to code that is not compiled yet share one exit per target, so more code fits the cache."),
         {"EDEN_JIT_COMPACT_EXITS"},
         {}},
    };
    return patches;
}

inline const Patch* Find(std::string_view id) {
    const auto& patches = List();
    const auto it = std::find_if(patches.begin(), patches.end(), [id](const Patch& patch) { return patch.id == id; });
    return it == patches.end() ? nullptr : &*it;
}

inline bool Recommended(const Patch& patch, std::uint64_t title) {
    return std::find(patch.recommended.begin(), patch.recommended.end(), title) != patch.recommended.end();
}

inline Eden::Settings::Json::json_pointer StateAt(std::uint64_t title, std::string_view id) {
    return Eden::Settings::Json::json_pointer("/games/" + Eden::Settings::TitleKey(title) + "/horizon_patches/" +
                                              std::string(id));
}

inline bool EnabledIn(const Eden::Settings::Json& document, std::uint64_t title, const Patch& patch) {
    return title != 0 && Eden::Settings::Bool(document, StateAt(title, patch.id), Recommended(patch, title));
}

inline bool IsEnabled(std::uint64_t title, std::string_view id, const std::string& file = Eden::SettingsFile()) {
    const Patch* patch = Find(id);
    return patch && EnabledIn(Eden::Settings::Load(file), title, *patch);
}

inline bool SetEnabled(std::uint64_t title, std::string_view id, bool enabled,
                       const std::string& file = Eden::SettingsFile()) {
    if (!title || !Find(id)) return false;
    Eden::Settings::Json document = Eden::Settings::Load(file);
    document["version"] = 1;
    auto& game = document["games"][Eden::Settings::TitleKey(title)];
    if (!game.is_object()) game = Eden::Settings::Json::object();
    auto& states = game["horizon_patches"];
    if (!states.is_object()) states = Eden::Settings::Json::object();
    states[std::string(id)] = enabled;
    return Eden::Settings::Write(document, file);
}

inline std::vector<const Patch*> EnabledFor(std::uint64_t title, const std::string& file = Eden::SettingsFile()) {
    const Eden::Settings::Json document = Eden::Settings::Load(file);
    std::vector<const Patch*> enabled;
    for (const auto& patch : List())
        if (EnabledIn(document, title, patch)) enabled.push_back(&patch);
    return enabled;
}

inline std::string Apply(std::uint64_t title, const std::string& file = Eden::SettingsFile()) {
    static const auto before = [] {
        std::vector<std::pair<std::string, std::optional<std::string>>> values;
        for (const auto& patch : List())
            for (const char* key : patch.keys) {
                const char* value = std::getenv(key);
                values.emplace_back(key, value ? std::optional<std::string>{value} : std::nullopt);
            }
        return values;
    }();
    for (const auto& [key, value] : before) {
        if (value) setenv(key.c_str(), value->c_str(), 1);
        else unsetenv(key.c_str());
    }
    std::string applied;
    for (const Patch* patch : EnabledFor(title, file)) {
        for (const char* key : patch->keys) setenv(key, "1", 1);
        applied += (applied.empty() ? "" : ", ") + std::string(patch->id);
    }
    return applied.empty() ? "none" : applied;
}

} // namespace Horizon::Patches
