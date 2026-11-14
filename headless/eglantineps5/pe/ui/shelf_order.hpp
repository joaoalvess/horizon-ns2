// EglantinePS5 - How the shelf orders its games.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pe::ui
{

// Favorites in pin order, then the last game played, then the other recents.
// A file already ranked keeps that rank. Anything else sorts after these.
inline std::unordered_map<std::string, int> ShelfRanks(const std::vector<std::string> &favorites,
                                                       std::string_view last,
                                                       const std::vector<std::string> &recents)
{
    std::unordered_map<std::string, int> rank;
    int next = 0;
    for (const std::string &file : favorites)
        rank.emplace(file, next++);
    if (!last.empty())
        rank.emplace(std::string{last}, next++);
    for (const std::string &file : recents)
        if (!file.empty())
            rank.emplace(file, next++);
    return rank;
}

inline int ShelfRank(const std::unordered_map<std::string, int> &rank, const std::string &file)
{
    const auto found = rank.find(file);
    return found == rank.end() ? 100000 : found->second;
}

} // namespace pe::ui
