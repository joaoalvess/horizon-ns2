// EglantinePS5 - Launcher home: the shelf of games.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pe/ui/launcher.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace pe::ui
{

using audio::Cue;

namespace
{

// Five covers across the 1920-wide screen, with the header above and the hints below.
constexpr int kCols = 5;
constexpr int kPage = kCols * 2;
constexpr float kGridLeft = 120.0f;
constexpr float kGridTop = 188.0f;
constexpr float kCell = 304.0f;
constexpr float kGap = 28.0f;
constexpr float kPitch = kCell + kGap;
constexpr float kGridBottom = 928.0f;

constexpr int kRowGames = 4;
constexpr float kRowLeft = 120.0f;
constexpr float kRowTop = 300.0f;
constexpr float kTile = 312.0f;
constexpr float kTileGap = 30.0f;
constexpr float kTileRadius = 22.0f;
constexpr int kDockItems = 6;
constexpr float kDockTop = 724.0f;
constexpr float kDockRadius = 52.0f;
constexpr float kDockItem = 124.0f;
constexpr float kDockGap = 44.0f;
constexpr float kTopY = 44.0f;
constexpr float kTopH = 72.0f;

constexpr const char *kDockNames[kDockItems] = {TR("Library"),     TR("Translations"), TR("Downloads"),
                                                TR("Multiplayer"), TR("Save data"),    TR("Settings")};

std::string initial(const std::string &name)
{
    if (name.empty())
        return {};
    std::size_t n = 1;
    while (n < name.size() && (static_cast<unsigned char>(name[n]) & 0xC0u) == 0x80u)
        ++n;
    std::string first = name.substr(0, n);
    if (n == 1 && first[0] >= 'a' && first[0] <= 'z')
        first[0] = static_cast<char>(first[0] - 'a' + 'A');
    return first;
}

void dock_icon(gfx::DrawList &list, int icon, float cx, float cy, float size, Color ink)
{
    const float u = size / 24.0f;
    const float ox = cx - 12.0f * u;
    const float oy = cy - 12.0f * u;
    const float w = 1.8f * u;
    const auto seg = [&](float x1, float y1, float x2, float y2)
    { list.line(ox + x1 * u, oy + y1 * u, ox + x2 * u, oy + y2 * u, w, ink); };
    const auto box = [&](float x, float y, float bw, float bh, float r)
    { list.bordered_rect({ox + x * u, oy + y * u, bw * u, bh * u}, r * u, ink.with_alpha(0.0f), w, ink); };
    const auto round = [&](float x, float y, float r) { list.ring(ox + x * u, oy + y * u, r * u, w, ink); };
    switch (icon)
    {
    case 0:
        box(3.0f, 3.0f, 7.0f, 7.0f, 2.0f);
        box(14.0f, 3.0f, 7.0f, 7.0f, 2.0f);
        box(3.0f, 14.0f, 7.0f, 7.0f, 2.0f);
        box(14.0f, 14.0f, 7.0f, 7.0f, 2.0f);
        return;
    case 1:
        seg(4.0f, 5.0f, 13.0f, 5.0f);
        seg(8.5f, 3.0f, 8.5f, 5.0f);
        seg(6.0f, 5.0f, 12.0f, 13.0f);
        seg(11.0f, 5.0f, 4.0f, 14.0f);
        seg(13.0f, 21.0f, 17.0f, 12.0f);
        seg(17.0f, 12.0f, 21.0f, 21.0f);
        seg(14.5f, 18.0f, 19.5f, 18.0f);
        return;
    case 2:
        seg(12.0f, 3.0f, 12.0f, 15.0f);
        seg(7.0f, 10.0f, 12.0f, 15.0f);
        seg(17.0f, 10.0f, 12.0f, 15.0f);
        seg(4.0f, 17.0f, 4.0f, 21.0f);
        seg(4.0f, 21.0f, 20.0f, 21.0f);
        seg(20.0f, 21.0f, 20.0f, 17.0f);
        return;
    case 3:
        round(8.0f, 9.0f, 3.0f);
        round(16.0f, 9.0f, 3.0f);
        seg(3.0f, 20.0f, 5.0f, 16.5f);
        seg(5.0f, 16.5f, 8.0f, 15.0f);
        seg(8.0f, 15.0f, 11.0f, 16.5f);
        seg(11.0f, 16.5f, 13.0f, 20.0f);
        seg(13.0f, 16.5f, 16.0f, 15.0f);
        seg(16.0f, 15.0f, 19.0f, 16.5f);
        seg(19.0f, 16.5f, 21.0f, 20.0f);
        return;
    case 4:
        box(4.0f, 4.0f, 16.0f, 16.0f, 2.0f);
        box(8.0f, 4.0f, 8.0f, 5.0f, 0.5f);
        box(7.0f, 13.0f, 10.0f, 7.0f, 1.0f);
        return;
    default:
        round(12.0f, 12.0f, 3.2f);
        round(12.0f, 12.0f, 7.0f);
        for (int k = 0; k < 8; ++k)
        {
            const float a = static_cast<float>(k) * 0.785398f;
            list.line(cx + 7.0f * u * std::cos(a), cy + 7.0f * u * std::sin(a), cx + 9.8f * u * std::cos(a),
                      cy + 9.8f * u * std::sin(a), 3.0f * u, ink);
        }
        return;
    }
}

void focus_ring(gfx::DrawList &list, const Rect &r, float radius)
{
    const Color ring = look().high_contrast ? th().text : th().accent;
    list.bordered_rect({r.x - 7.5f, r.y - 7.5f, r.w + 15.0f, r.h + 15.0f}, radius + 7.5f,
                       th().base.with_alpha(0.0f), 5.0f, ring);
}

} // namespace

bool Launcher::select_shelf(int index)
{
    const int count = static_cast<int>(games_.size());
    if (count <= 0)
        return false;
    index = std::clamp(index, 0, count - 1);
    if (!shelf_settings_ && !shelf_all_ && index == library_.selected)
        return false;
    shelf_settings_ = false;
    shelf_all_ = false;
    library_.selected = index;
    library_.count = count;
    refresh_selected_game();
    mode_.snap(selected_docked_ ? 0.0f : 1.0f);
    detail_.value = 0.0f;
    detail_.velocity = 0.0f;
    cue(Cue::focus);
    return true;
}

void Launcher::update_shelf(float dt)
{
    const int count = static_cast<int>(games_.size());
    if (count == 0)
    {
        shelf_settings_ = true;
        shelf_grid_ = false;
    }
    else if (library_.selected < 0 || library_.selected >= count)
        library_.selected = std::clamp(library_.selected, 0, count - 1);
    if (!shelf_grid_ && !shelf_settings_ && library_.selected >= std::min(count, kRowGames))
        shelf_all_ = true;
    const int rows = count == 0 ? 0 : (count + kCols - 1) / kCols;
    const float view = kGridBottom - kGridTop;
    const float content = static_cast<float>(rows) * kPitch - kGap;
    const float max_scroll = std::max(0.0f, content - view);
    const int row = !shelf_grid_ || count == 0 ? 0 : library_.selected / kCols;
    const float focus_top = static_cast<float>(row) * kPitch;
    float target = shelf_scroll_.target;
    if (focus_top < target)
        target = focus_top;
    if (focus_top + kCell > target + view)
        target = focus_top + kCell - view;
    shelf_scroll_.target = std::clamp(target, 0.0f, max_scroll);
    shelf_scroll_.update(dt, th().scroll_spring);
}

void Launcher::open_dock_item()
{
    press_ = 1.0f;
    switch (dock_)
    {
    case 0:
        if (games_.empty())
        {
            cue(Cue::error);
            return;
        }
        shelf_grid_ = true;
        shelf_settings_ = false;
        shelf_all_ = false;
        cue(Cue::open);
        return;
    case 1:
        open_translations();
        return;
    case 2:
        open_sources();
        return;
    case 3:
        open_multiplayer();
        return;
    case 4:
        open_sync_setup();
        return;
    default:
        open(Screen::settings, true);
        prefs_ = services_.preferences(platform_);
        section_.snap(1.0f);
        return;
    }
}

void Launcher::press_home(Key key)
{
    if (key == Key::l1 || key == Key::r1)
    {
        choose_platform(key == Key::r1 ? 1 : 0);
        return;
    }
    if (shelf_grid_)
    {
        press_shelf_grid(key);
        return;
    }
    const int count = static_cast<int>(games_.size());
    if (count == 0)
        shelf_settings_ = true;
    else if (library_.selected < 0 || library_.selected >= count)
        library_.selected = std::clamp(library_.selected, 0, count - 1);
    const int shown = std::min(count, kRowGames);
    if (!shelf_settings_ && library_.selected >= shown)
        shelf_all_ = true;

    if (shelf_settings_)
    {
        switch (key)
        {
        case Key::left:
        case Key::right:
        {
            const int next = dock_ + (key == Key::right ? 1 : -1);
            if (next < 0 || next >= kDockItems)
                return;
            dock_ = next;
            cue(Cue::focus);
            return;
        }
        case Key::up:
            if (count == 0)
            {
                cue(Cue::error);
                return;
            }
            shelf_settings_ = false;
            cue(Cue::focus);
            return;
        case Key::cross:
            open_dock_item();
            return;
        default:
            return;
        }
    }

    const Game *game = shelf_all_ ? nullptr : &games_[static_cast<std::size_t>(library_.selected)];
    switch (key)
    {
    case Key::down:
        shelf_settings_ = true;
        cue(Cue::focus);
        return;
    case Key::left:
        if (shelf_all_)
        {
            if (!select_shelf(shown - 1))
                cue(Cue::focus);
        }
        else if (library_.selected > 0)
            select_shelf(library_.selected - 1);
        return;
    case Key::right:
        if (shelf_all_)
            return;
        if (library_.selected + 1 < shown)
            select_shelf(library_.selected + 1);
        else
        {
            shelf_all_ = true;
            cue(Cue::focus);
        }
        return;
    case Key::square:
        if (game == nullptr || !home_.setup_ready)
        {
            cue(Cue::error);
            return;
        }
        {
            const bool on = std::find(favorites_.begin(), favorites_.end(), game->file) == favorites_.end();
            if (!services_.set_favorite(game->file, on))
            {
                const bool full = on && favorites_.size() >= 24;
                say(full ? tr("The shelf already has 24 pinned games.") :
                           on ? tr("This game cannot be pinned.") : tr("Could not update the shelf."),
                    true);
                cue(Cue::error);
                return;
            }
            favorites_ = services_.favorites();
            reseat_shelf();
            cue(on ? Cue::saved : Cue::toggle);
        }
        return;
    case Key::triangle:
        if (game == nullptr)
        {
            cue(Cue::error);
            return;
        }
        open_game_panel();
        return;
    case Key::cross:
        press_ = 1.0f;
        if (game == nullptr)
        {
            shelf_grid_ = true;
            cue(Cue::open);
            return;
        }
        if (!home_.setup_ready)
        {
            cue(Cue::error);
            return;
        }
        if (game->remote)
        {
            play_remote(*game);
            return;
        }
        launch(game->file, game->name, game->cover);
        return;
    default:
        return;
    }
}

void Launcher::press_shelf_grid(Key key)
{
    const int count = static_cast<int>(games_.size());
    if (count == 0)
    {
        shelf_grid_ = false;
        return;
    }
    const Game *game = &games_[static_cast<std::size_t>(library_.selected)];
    switch (key)
    {
    case Key::circle:
        shelf_grid_ = false;
        shelf_all_ = library_.selected >= std::min(count, kRowGames);
        cue(Cue::back);
        return;
    case Key::up:
        if (library_.selected >= kCols)
            select_shelf(library_.selected - kCols);
        return;
    case Key::down:
    {
        int next = library_.selected + kCols;
        if (next >= count)
        {
            const int last_row = (count - 1) / kCols;
            if (library_.selected / kCols < last_row)
                next = count - 1;
            else
                return;
        }
        select_shelf(next);
        return;
    }
    case Key::left:
        if (library_.selected % kCols == 0)
            return;
        select_shelf(library_.selected - 1);
        return;
    case Key::right:
        if (library_.selected % kCols == kCols - 1 || library_.selected + 1 >= count)
            return;
        select_shelf(library_.selected + 1);
        return;
    case Key::l2:
        select_shelf(std::max(0, library_.selected - kPage));
        return;
    case Key::r2:
        select_shelf(std::min(count - 1, library_.selected + kPage));
        return;
    case Key::square:
    {
        if (!home_.setup_ready)
        {
            cue(Cue::error);
            return;
        }
        const bool on = std::find(favorites_.begin(), favorites_.end(), game->file) == favorites_.end();
        if (!services_.set_favorite(game->file, on))
        {
            const bool full = on && favorites_.size() >= 24;
            say(full ? tr("The shelf already has 24 pinned games.") :
                       on ? tr("This game cannot be pinned.") : tr("Could not update the shelf."),
                true);
            cue(Cue::error);
            return;
        }
        favorites_ = services_.favorites();
        reseat_shelf();
        cue(on ? Cue::saved : Cue::toggle);
        return;
    }
    case Key::triangle:
        open_game_panel();
        return;
    case Key::cross:
        press_ = 1.0f;
        if (!home_.setup_ready)
        {
            cue(Cue::error);
            return;
        }
        if (game->remote)
        {
            play_remote(*game);
            return;
        }
        launch(game->file, game->name, game->cover);
        return;
    default:
        return;
    }
}

void Launcher::update_controllers(float dt)
{
    const unsigned now = services_.controllers() & 0xfu;
    bool joined = false;
    bool left = false;
    for (int player = 0; player < 4; ++player)
    {
        const std::size_t index = static_cast<std::size_t>(player);
        const bool on = (now >> player & 1u) != 0;
        controller_lit_[index].target = on ? 1.0f : 0.0f;
        if (!controllers_known_)
        {
            controller_lit_[index].snap(controller_lit_[index].target);
        }
        else if (on != ((controllers_ >> player & 1u) != 0))
        {
            (on ? joined : left) = true;
            if (on)
                controller_pop_[index] = 1.0f;
        }
        controller_lit_[index].update(dt, 10.0f);
        controller_pop_[index] = std::max(0.0f, controller_pop_[index] - dt / 0.5f);
    }
    // One sound however many changed at once; none while a game is starting.
    if ((joined || left) && selected_game_.empty())
        cue(joined ? Cue::saved : Cue::modal_close);
    controllers_ = now;
    controllers_known_ = true;
}

void Launcher::draw_profile_bar(Canvas &c, bool platforms)
{
    gfx::DrawList &list = c.list;
    const float top_cy = kTopY + kTopH * 0.5f;

    const std::string who = playing_.empty() ? std::string{"Horizon"} : playing_;
    list.shadow({96.0f, top_cy - 26.0f, 64.0f, 64.0f}, 32.0f, 18.0f, th().scrim.with_alpha(0.08f));
    list.circle(128.0f, top_cy, 32.0f, th().panel);
    list.circle(128.0f, top_cy, 29.0f, th().row_focus);
    text(c, initial(who), 128.0f, baseline(top_cy - 32.0f, 64.0f, 28.0f), 28.0f, th().accent_deep, Align::center);
    text_shrink(c, who, 176.0f, baseline(kTopY, kTopH, 26.0f), 26.0f, th().title, 380.0f);

    const std::string names[2] = {"Switch", "Switch 2"};
    float pill[2];
    for (int i = 0; i < 2; ++i)
        pill[i] = text_width(c, names[i], 22.0f) + 90.0f;
    const float key_w = 52.0f;
    const float track_w = pill[0] + pill[1] + 12.0f;
    const float total = key_w * 2.0f + 36.0f + track_w;
    float x = 960.0f - total * 0.5f;
    const auto keycap = [&](std::string_view label, float left)
    {
        list.bordered_rect({left, top_cy - 18.0f, key_w, 36.0f}, 10.0f, th().panel, 2.0f, th().toggle_track);
        text(c, label, left + key_w * 0.5f, baseline(top_cy - 18.0f, 36.0f, 16.0f), 16.0f, th().meta,
             Align::center);
    };
    if (!platforms)
    {
        text(c, clock_, 1824.0f, baseline(kTopY, kTopH, 26.0f), 26.0f, th().title, Align::right);
        return;
    }
    keycap("L1", x);
    x += key_w + 18.0f;
    const Rect track{x, top_cy - 29.0f, track_w, 58.0f};
    list.rounded_rect(track, 29.0f, th().row_edge);
    float px = track.x + 6.0f;
    for (int i = 0; i < 2; ++i)
    {
        const bool on = platform_ == i;
        const Rect r{px, track.y + 6.0f, pill[i], 46.0f};
        if (on)
        {
            list.shadow({r.x + 2.0f, r.y + 4.0f, r.w - 4.0f, r.h}, 23.0f, 12.0f, th().scrim.with_alpha(0.08f));
            list.rounded_rect(r, 23.0f, th().panel);
            list.circle(r.x + 39.0f, r.y + 23.0f, 5.0f, th().accent);
        }
        text(c, names[i], r.x + 56.0f, baseline(r.y, r.h, 22.0f), 22.0f, on ? th().title : th().meta);
        px += pill[i];
    }
    keycap("R1", track.x + track.w + 18.0f);
    text(c, clock_, 1824.0f, baseline(kTopY, kTopH, 26.0f), 26.0f, th().title, Align::right);

}

float Launcher::draw_brand_bar(Canvas &c)
{
    gfx::DrawList &list = c.list;
    list.rounded_rect({120.0f, 952.0f, 1680.0f, 1.0f}, 0.0f, th().rule);
    float x = 120.0f;
    if (const std::uint32_t brand = c.textures.brand(); brand != 0)
    {
        list.rounded_image(brand, {x, 962.0f, 40.0f, 40.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, 10.0f, th().panel);
        x += 54.0f;
    }
    return x + text(c, "Horizon", x, 989.0f, th().size_small, th().title) + 48.0f;
}

void Launcher::draw_home(Canvas &c)
{
    if (shelf_grid_)
    {
        draw_shelf_grid(c);
        return;
    }
    gfx::DrawList &list = c.list;
    const int count = static_cast<int>(games_.size());
    const int selected = count == 0 ? -1 : std::clamp(library_.selected, 0, count - 1);
    const int shown = std::min(count, kRowGames);
    const Game *game = selected >= 0 ? &games_[static_cast<std::size_t>(selected)] : nullptr;
    const bool alert_warning = message_warning_ || home_.launch_failed;
    const std::string alert = !message_.empty() ? message_ : home_.status;
    draw_profile_bar(c, true);

    const float featured = 256.0f;
    float sub_x = kRowLeft;
    if (count > 0)
    {
        const std::string title = shelf_all_ || game == nullptr ? std::string{tr("All games")} : game->name;
        sub_x += text_shrink(c, title, kRowLeft, featured, th().size_lead, th().title, 980.0f) + 24.0f;
    }
    if (!alert.empty())
        notice(c, alert, sub_x, featured, 22.0f, alert_warning ? th().warning : th().meta,
               1800.0f - sub_x, alert_warning);
    else if (shelf_all_ && count > 0)
        text(c, fill(tr("{0} games"), {std::to_string(count)}), sub_x, featured, 22.0f, th().meta);
    else if (game != nullptr)
    {
        const bool pinned = std::find(favorites_.begin(), favorites_.end(), game->file) != favorites_.end();
        std::string meta = game->remote ? remote_state(*game, nullptr) : game->format;
        if (!game->remote && !game->language.empty())
            meta += std::string{"  /  "} + game->language;
        if (pinned)
            meta += std::string{"  /  "} + tr("Pinned");
        if (!services_.game_profile(game->title_id).empty())
            meta += std::string{"  /  "} + tr("Recommended");
        text_shrink(c, meta, sub_x, featured, 22.0f, th().meta, 1800.0f - sub_x);
    }

    if (count == 0)
    {
        text(c, games_loaded_ ? tr("No ROM files found.") : tr("Reading..."), 960.0f,
             baseline(kRowTop, kTile, th().size_text), th().size_text, th().copy, Align::center);
    }
    else
    {
        const int focus = shelf_settings_ ? -1 : shelf_all_ ? shown : selected;
        for (int i = 0; i <= shown; ++i)
        {
            const float tx = kRowLeft + static_cast<float>(i) * (kTile + kTileGap);
            const Rect cell{tx, kRowTop, kTile, kTile};
            const bool focused = i == focus;
            if (focused)
            {
                list.push_transform(1.0f + 0.045f * motion(), tx + kTile * 0.5f, kRowTop + kTile * 0.5f, 0.0f, 0.0f);
                list.shadow({tx + 6.0f, kRowTop + 18.0f, kTile - 12.0f, kTile}, kTileRadius, 40.0f,
                            th().accent_deep.with_alpha(0.28f));
            }
            if (i < shown)
            {
                const Game &item = games_[static_cast<std::size_t>(i)];
                if (item.remote)
                    remote_cover(c, item.cover, cell, kTileRadius, 0.5f, item.name);
                else
                    cover(c, item.cover, cell, kTileRadius, 0.5f, item.name);
                if (std::find(favorites_.begin(), favorites_.end(), item.file) != favorites_.end())
                    list.circle(tx + kTile - 28.0f, kRowTop + 28.0f, 11.0f, th().accent);
                const Download *download = item.remote ? download_of(item.key) : nullptr;
                if (download != nullptr &&
                    (download->state == DownloadState::downloading || download->state == DownloadState::verifying) &&
                    download->total > 0)
                {
                    const float done = std::clamp(static_cast<float>(static_cast<double>(download->done) /
                                                                     static_cast<double>(download->total)),
                                                  0.0f, 1.0f);
                    const float bar = kTile - 36.0f;
                    list.rounded_rect({tx + 18.0f, kRowTop + kTile - 24.0f, bar, 6.0f}, 3.0f,
                                      th().scrim.with_alpha(0.72f));
                    list.rounded_rect({tx + 18.0f, kRowTop + kTile - 24.0f, std::max(6.0f, bar * done), 6.0f}, 3.0f,
                                      th().accent);
                }
            }
            else
            {
                list.bordered_rect(cell, kTileRadius, th().panel, 2.0f, th().toggle_track);
                dock_icon(list, 0, tx + kTile * 0.5f, kRowTop + 130.0f, 64.0f, focused ? th().accent : th().meta);
                text_shrink(c, tr("All games"), tx + kTile * 0.5f, kRowTop + 212.0f, th().size_text,
                            focused ? th().title : th().meta, kTile - 40.0f, Align::center);
            }
            if (focused)
            {
                focus_ring(list, cell, kTileRadius);
                list.pop_transform();
            }
        }
    }

    const float dock_left = 960.0f - (kDockItem * kDockItems + kDockGap * (kDockItems - 1)) * 0.5f;
    for (int i = 0; i < kDockItems; ++i)
    {
        const float cx = dock_left + static_cast<float>(i) * (kDockItem + kDockGap) + kDockItem * 0.5f;
        const float cy = kDockTop + kDockRadius;
        const bool focused = shelf_settings_ && i == dock_;
        const Rect disc{cx - kDockRadius, cy - kDockRadius, kDockRadius * 2.0f, kDockRadius * 2.0f};
        list.shadow({disc.x, disc.y + 6.0f, disc.w, disc.h}, kDockRadius, focused ? 36.0f : 18.0f,
                    focused ? th().accent_deep.with_alpha(0.28f) : th().scrim.with_alpha(0.08f));
        list.circle(cx, cy, kDockRadius, th().panel);
        if (focused)
            focus_ring(list, disc, kDockRadius);
        dock_icon(list, i, cx, cy, 46.0f, focused ? th().accent : th().title);
        text_shrink(c, tr(kDockNames[i]), cx, baseline(cy + kDockRadius + 14.0f, 28.0f, 19.0f), 19.0f,
                    focused ? th().title : th().meta, kDockItem + kDockGap - 8.0f, Align::center);
    }

    const float hints_x = draw_brand_bar(c);
    static constexpr Hint kDockHints[] = {{Pad::cross, TR("Select")},
                                          {Pad::dpad, TR("Navigate")},
                                          {Pad::l1, TR("Switch / Switch 2"), Pad::r1}};
    static constexpr Hint kGameHints[] = {{Pad::cross, TR("Select")},
                                          {Pad::triangle, TR("Details")},
                                          {Pad::square, TR("Favorite")},
                                          {Pad::dpad, TR("Navigate")},
                                          {Pad::l1, TR("Switch / Switch 2"), Pad::r1}};
    static constexpr Hint kRemoteHints[] = {{Pad::cross, TR("Download and play")},
                                            {Pad::triangle, TR("Details")},
                                            {Pad::square, TR("Favorite")},
                                            {Pad::dpad, TR("Navigate")},
                                            {Pad::l1, TR("Switch / Switch 2"), Pad::r1}};
    const Hint *hints = kGameHints;
    int hint_count = 5;
    if (shelf_settings_ || shelf_all_ || game == nullptr)
    {
        hints = kDockHints;
        hint_count = 3;
    }
    else if (game->remote)
    {
        hints = kRemoteHints;
    }
    std::string status = home_.system_status;
    if (services_.room_status().phase == Eden::Multiplayer::Phase::Connected)
        status += std::string{"  /  "} + tr("Multiplayer") + ": " + tr("Connected");
    const float status_width =
        text_shrink(c, status, 1800.0f, 989.0f, th().size_small, th().faint, 600.0f, Align::right);
    draw_hints(c, hints, hint_count, hints_x, 982.0f, th().muted, 1800.0f - hints_x - status_width - 48.0f);
}

void Launcher::draw_shelf_grid(Canvas &c)
{
    gfx::DrawList &list = c.list;
    const int count = static_cast<int>(games_.size());
    const int selected = count == 0 ? -1 : std::clamp(library_.selected, 0, count - 1);
    const Game *game = selected >= 0 ? &games_[static_cast<std::size_t>(selected)] : nullptr;
    const bool alert_warning = message_warning_ || home_.launch_failed;
    const std::string alert = !message_.empty() ? message_ : home_.status;

    text(c, tr("All games"), 120.0f, baseline(64.0f, 44.0f, th().size_brand), th().size_brand, th().text,
         Align::left, 1.2f);
    text(c, platform_ == 1 ? "Switch 2" : "Switch", 120.0f, baseline(112.0f, 26.0f, th().size_small), th().size_small,
         th().meta);
    text(c, clock_, 1800.0f, baseline(64.0f, 36.0f, th().size_clock), th().size_clock, th().meta, Align::right);

    if (game != nullptr)
        text_shrink(c, game->name, 960.0f, baseline(68.0f, 44.0f, th().size_heading), th().size_heading,
                    th().title, 760.0f, Align::center);
    if (!alert.empty())
        notice(c, alert, 960.0f, baseline(118.0f, 28.0f, th().size_small), th().size_small,
               alert_warning ? th().warning : th().meta, 760.0f, alert_warning, Align::center);
    else if (game != nullptr)
    {
        const bool pinned =
            std::find(favorites_.begin(), favorites_.end(), game->file) != favorites_.end();
        std::string meta = game->remote ? remote_state(*game, nullptr) : game->format;
        if (!game->remote && !game->language.empty())
            meta += std::string{"  /  "} + game->language;
        if (pinned)
            meta += std::string{"  /  "} + tr("Pinned");
        if (!services_.game_profile(game->title_id).empty())
            meta += std::string{"  /  "} + tr("Recommended");
        text_shrink(c, meta, 960.0f, baseline(118.0f, 28.0f, th().size_small), th().size_small, th().meta,
                    760.0f, Align::center);
    }

    const float scroll = shelf_scroll_.value;
    list.push_clip({96.0f, kGridTop - 12.0f, 1728.0f, kGridBottom - kGridTop + 24.0f});
    for (int i = 0; i < count; ++i)
    {
        const Game &item = games_[static_cast<std::size_t>(i)];
        const int col = i % kCols;
        const int row = i / kCols;
        const float x = kGridLeft + static_cast<float>(col) * kPitch;
        const float y = kGridTop + static_cast<float>(row) * kPitch - scroll;
        if (y + kCell < kGridTop - 8.0f || y > kGridBottom)
            continue;
        const bool focused = i == selected;
        const float cx = x + kCell * 0.5f;
        const float cy = y + kCell * 0.5f;
        if (focused)
            list.push_transform(1.0f + 0.035f * motion(), cx, cy, 0.0f, -3.0f * motion());
        const Rect cell{x, y, kCell, kCell};
        if (item.remote)
            remote_cover(c, item.cover, cell, 10.0f, 0.0f, item.name);
        else
            cover(c, item.cover, cell, 10.0f, 0.0f, item.name);
        if (std::find(favorites_.begin(), favorites_.end(), item.file) != favorites_.end())
            list.circle(x + kCell - 28.0f, y + 28.0f, 11.0f, th().accent);
        const Download *download = item.remote ? download_of(item.key) : nullptr;
        if (download != nullptr &&
            (download->state == DownloadState::downloading || download->state == DownloadState::verifying) &&
            download->total > 0)
        {
            const float done = std::clamp(static_cast<float>(static_cast<double>(download->done) /
                                                             static_cast<double>(download->total)),
                                          0.0f, 1.0f);
            const float bar = kCell - 36.0f;
            list.rounded_rect({x + 18.0f, y + kCell - 24.0f, bar, 6.0f}, 3.0f, th().scrim.with_alpha(0.72f));
            list.rounded_rect({x + 18.0f, y + kCell - 24.0f, std::max(6.0f, bar * done), 6.0f}, 3.0f,
                              th().accent);
        }
        if (focused)
        {
            focus_ring(list, cell, 10.0f);
            list.pop_transform();
        }
    }
    list.pop_clip();

    list.rounded_rect({120.0f, 952.0f, 1680.0f, 1.0f}, 0.0f, th().rule);
    static constexpr Hint kGameHints[] = {{Pad::cross, TR("Select")},
                                          {Pad::circle, TR("Back")},
                                          {Pad::triangle, TR("Details")},
                                          {Pad::square, TR("Favorite")},
                                          {Pad::l2, TR("Page"), Pad::r2},
                                          {Pad::l1, TR("Switch / Switch 2"), Pad::r1}};
    static constexpr Hint kRemoteHints[] = {{Pad::cross, TR("Download and play")},
                                            {Pad::circle, TR("Back")},
                                            {Pad::triangle, TR("Details")},
                                            {Pad::square, TR("Favorite")},
                                            {Pad::l2, TR("Page"), Pad::r2},
                                            {Pad::l1, TR("Switch / Switch 2"), Pad::r1}};
    const Hint *hints = game != nullptr && game->remote ? kRemoteHints : kGameHints;
    std::string status;
    if (selected >= 0)
        status = list_position(selected + 1, count);
    if (!home_.system_status.empty())
    {
        if (!status.empty())
            status += "  /  ";
        status += home_.system_status;
    }
    const float status_width =
        text_shrink(c, status, 1800.0f, 989.0f, th().size_small, th().faint, 900.0f, Align::right);
    draw_hints(c, hints, 6, 120.0f, 982.0f, th().muted, 1680.0f - status_width - 48.0f);
}

} // namespace pe::ui
