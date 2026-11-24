// SPDX-License-Identifier: GPL-3.0-or-later
#include "horizon_patches.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <unistd.h>

static std::string Read(const std::string& path) {
    std::ifstream in(path);
    return {std::istreambuf_iterator<char>(in), {}};
}

static bool KeyIs(const char* key, const char* value) {
    const char* current = std::getenv(key);
    return value ? current && std::strcmp(current, value) == 0 : current == nullptr;
}

int main() {
    namespace P = Horizon::Patches;
    char directory[] = "/tmp/horizon-patches-XXXXXX";
    assert(mkdtemp(directory));
    const std::string file = std::string(directory) + "/eglantineps5.json";
    constexpr std::uint64_t game = 0x0100000000010000, other = 0x0100000000020000;

    for (const auto& patch : P::List()) {
        assert(patch.id && *patch.id && patch.name && *patch.name && patch.description && *patch.description);
        assert(!patch.keys.empty() && P::Find(patch.id) == &patch);
        for (const char* c = patch.id; *c; ++c) assert((*c >= 'a' && *c <= 'z') || *c == '_');
    }
    assert(!P::Find("missing"));
    const char* key = P::Find("compact_exits")->keys.front();

    assert(!P::IsEnabled(game, "compact_exits", file) && P::EnabledFor(game, file).empty());
    assert(!P::SetEnabled(0, "compact_exits", true, file) && !P::SetEnabled(game, "missing", true, file));
    assert(P::SetEnabled(game, "compact_exits", true, file) && P::IsEnabled(game, "compact_exits", file));
    assert(!P::IsEnabled(other, "compact_exits", file) && !P::IsEnabled(0, "compact_exits", file));
    assert(Read(file).find("\"horizon_patches\"") != std::string::npos);
    assert(P::SetEnabled(game, "compact_exits", false, file) && !P::IsEnabled(game, "compact_exits", file));
    assert(Read(file).find("\"compact_exits\": false") != std::string::npos);

    const P::Patch recommended{"fake", "Fake", "Fake", {"HORIZON_FAKE"}, {game}};
    Eden::Settings::Json document = Eden::Settings::Json::object();
    assert(P::EnabledIn(document, game, recommended) && !P::EnabledIn(document, other, recommended));
    document["games"][Eden::Settings::TitleKey(game)]["horizon_patches"]["fake"] = false;
    assert(!P::EnabledIn(document, game, recommended));
    document["games"][Eden::Settings::TitleKey(other)]["horizon_patches"]["fake"] = true;
    assert(P::EnabledIn(document, other, recommended));

    setenv(key, "0", 1);
    assert(P::SetEnabled(game, "compact_exits", true, file));
    assert(P::Apply(game, file) == "compact_exits" && KeyIs(key, "1"));
    assert(P::Apply(other, file) == "none" && KeyIs(key, "0"));
    assert(P::Apply(game, file) == "compact_exits" && KeyIs(key, "1"));
    assert(P::Apply(0, file) == "none" && KeyIs(key, "0"));

    std::remove(file.c_str());
    rmdir(directory);
    std::puts("Horizon patches: registry, per-game state and environment PASS");
    return 0;
}
