// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
// AstraPS5 native shell. Flatpak game launch; Steam entries are discovery-only.
#include "astraps5/menu.hpp"
#include "astraps5/menu_state.hpp"
#include "astraps5/diagnostics.hpp"
#include "astraps5/game.hpp"
#include "astraps5/library.hpp"
#include "astraps5/steam.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <poll.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace astra {
namespace {
constexpr int kMinWidth = 540;
constexpr int kMinHeight = 620;
constexpr int kRowHeight = 54;

struct Palette {
    unsigned long background, panel, panelHover, primary, subdued, accent, border;
};

unsigned long rgb(Display* display, const char* value) {
    XColor color{};
    Colormap map = DefaultColormap(display, DefaultScreen(display));
    if (!XParseColor(display, map, value, &color) || !XAllocColor(display, map, &color)) {
        return WhitePixel(display, DefaultScreen(display));
    }
    return color.pixel;
}

struct App {
    Display* display = nullptr;
    Window window = 0;
    GC gc = nullptr;
    XFontStruct* heading = nullptr;
    XFontStruct* normal = nullptr;
    XFontStruct* small = nullptr;
    Atom deleteWindow = None;
    int width = 900;
    int height = 620;
    MenuState state{};
    Palette colors{};
    std::vector<std::string> report;
    GameLibrary library{};
    SteamDiscovery steam{};
    // Menu index covers both Flatpak entries (launchable) and Steam manifests
    // (discovery-only until Steam integration has been validated).
    struct MenuGame {
        std::string name;
        std::string subtitle;
        bool steam_entry = false;
        std::size_t source_index = 0;
    };
    std::vector<MenuGame> entries;
    std::string runningGame;
    pid_t gamePid = -1;
    std::filesystem::path gameLog;
    std::string gameStatus;

    static constexpr int kHomeActions = 3;
    static constexpr int kGameListTop = 182;
    static constexpr int kListStride = kRowHeight + 12;

    int maxVisibleGames() const {
        return std::max(1, (height - 115 - kGameListTop) / kListStride + 1);
    }
    int firstVisibleGame() const {
        return std::max(0, static_cast<int>(state.selected_game) - maxVisibleGames() + 1);
    }
    const char* actionLabel(MenuAction action) const {
        switch (action) {
            case MenuAction::Games: return "Games";
            case MenuAction::Diagnostics: return "Diagnostics";
            case MenuAction::Exit: return "Exit";
        }
        return "Unknown";
    }
    // The simple X11 fonts used by this prototype don't support UTF-8.
    static std::string displayLabel(const std::string& original, std::size_t limit = 40) {
        std::string result;
        for (unsigned char ch : original) {
            if (result.size() >= limit) break;
            result += ch >= 32 && ch <= 126 ? static_cast<char>(ch) : '?';
        }
        if (original.size() > limit && result.size() >= 3) result.replace(result.size()-3, 3, "...");
        return result;
    }
    void refreshLibrary() {
        library = discover_flatpak_games();
        steam = discover_steam();
        entries.clear();
        for (std::size_t i = 0; i < library.games.size(); ++i) {
            entries.push_back({library.games[i].name, "Flatpak / " + library.games[i].flatpak_id, false, i});
        }
        for (std::size_t i = 0; i < steam.games.size(); ++i) {
            entries.push_back({steam.games[i].name, "Steam / " + steam.games[i].app_id + " (discovery only)", true, i});
        }
        std::sort(entries.begin(), entries.end(), [](const MenuGame& a, const MenuGame& b) {
            return a.name < b.name || (a.name == b.name && a.subtitle < b.subtitle);
        });
        state.game_count = entries.size();
        if (state.game_count == 0) state.selected_game = 0;
        else if (state.selected_game >= state.game_count) state.selected_game = state.game_count - 1;
    }
    void requestLaunch() {
        if (!state.launch_requested) return;
        state.launch_requested = false;
        if (state.selected_game >= entries.size()) return;
        const auto& entry = entries[state.selected_game];
        if (entry.steam_entry) {
            gameStatus = "Steam title detected. Game launching is not implemented yet.";
            return;
        }
        const auto& candidate = library.games[entry.source_index];
        if (gamePid > 0) {
            gameStatus = runningGame + " is already running. Close it before starting another game.";
            return;
        }
        // Recheck the installed app before spawning; never execute .desktop Exec.
        const auto verified = inspect_flatpak_app(candidate.name, candidate.flatpak_id);
        if (!verified.valid) {
            gameStatus = "Game unavailable; press R to refresh the library.";
            return;
        }
        const auto started = start_game(verified);
        if (!started.started()) {
            gameStatus = "Launch failed: " + started.error;
            std::cerr << gameStatus << '\n';
            return;
        }
        runningGame = verified.name;
        gamePid = started.pid;
        gameLog = started.log_path;
        gameStatus = runningGame + " running; log: " + gameLog.filename().string();
        std::cout << "Game log: " << gameLog << '\n';
    }
    void checkGame() {
        if (gamePid <= 0) return;
        const auto status = poll_game(gamePid);
        if (status.state == GameProcessState::Running) return;
        gamePid = -1;
        gameStatus = runningGame + ": " + status.message + " Log: " + gameLog.filename().string();
        draw();
    }

    int origin_x() const { return std::max(30, (width - 480) / 2); }
    int item_y() const { return height / 2 + 20; }

    void rectangle(int x, int y, int w, int h, unsigned long color) const {
        XSetForeground(display, gc, color);
        XFillRectangle(display, window, gc, x, y,
                       static_cast<unsigned int>(std::max(0, w)),
                       static_cast<unsigned int>(std::max(0, h)));
    }
    void line(int x, int y, int x2, int y2, unsigned long color) const {
        XSetForeground(display, gc, color);
        XDrawLine(display, window, gc, x, y, x2, y2);
    }
    void text(int x, int y, const std::string& content, unsigned long color,
              XFontStruct* font) const {
        XSetForeground(display, gc, color);
        if (font) XSetFont(display, gc, font->fid);
        XDrawString(display, window, gc, x, y, content.c_str(), static_cast<int>(content.size()));
    }
    void centered(int center, int y, const std::string& content, unsigned long color,
                  XFontStruct* font) const {
        const int textWidth = font ? XTextWidth(font, content.c_str(), static_cast<int>(content.size()))
                                   : static_cast<int>(content.size() * 8);
        text(center - textWidth / 2, y, content, color, font);
    }
    void frame() const {
        rectangle(0, 0, width, height, colors.background);
        rectangle(0, 0, width, 3, colors.accent);
        const std::string footer = "ASTRAPS5  /  EXPERIMENTAL TEST PROJECT  /  XENO TECH";
        centered(width / 2, height - 30, footer, colors.subdued, small);
    }
    void home() const {
        centered(width / 2, height / 2 - 145, "ASTRAPS5", colors.primary, heading);
        centered(width / 2, height / 2 - 111, "EXPERIMENTAL LINUX GAME LAUNCHER", colors.subdued, small);
        line(width / 2 - 55, height / 2 - 87, width / 2 + 55, height / 2 - 87, colors.accent);
        const int x = origin_x(), y = item_y();
        constexpr MenuAction actions[] = {MenuAction::Games, MenuAction::Diagnostics, MenuAction::Exit};
        for (int i = 0; i < kHomeActions; ++i) {
            const bool current = state.selected == actions[i];
            rectangle(x, y + i * kListStride, 480, kRowHeight,
                      current ? colors.panelHover : colors.panel);
            if (current) rectangle(x, y + i * kListStride, 4, kRowHeight, colors.accent);
            std::string label = actionLabel(actions[i]);
            if (actions[i] == MenuAction::Games)
                label += " (" + std::to_string(entries.size()) + ")";
            text(x + 24, y + 34 + i * kListStride, label,
                 current ? colors.primary : colors.subdued, normal);
            if (current) text(x + 443, y + 34 + i * kListStride, ">", colors.accent, normal);
        }
        centered(width / 2, height - 65, "UP / DOWN  -  SELECT       ENTER  -  OPEN       ESC  -  QUIT",
                 colors.subdued, small);
    }
    void games() const {
        const int x = origin_x();
        centered(width / 2, 80, "GAMES", colors.primary, heading);
        centered(width / 2, 111, "FLATPAK LAUNCH / STEAM DISCOVERY / PROTON: " + std::to_string(steam.protons.size()), colors.accent, small);
        line(x, 134, x + 480, 134, colors.border);
        if (entries.empty()) {
            centered(width / 2, height / 2 - 25, "No installed games found.", colors.primary, normal);
            centered(width / 2, height / 2 + 5, "Install a Flatpak or Steam title, then press R.", colors.subdued, small);
        } else {
            const int first = firstVisibleGame();
            for (int i = 0; i < maxVisibleGames() && first + i < static_cast<int>(entries.size()); ++i) {
                const int index = first + i;
                const auto& game = entries[static_cast<std::size_t>(index)];
                const int y = kGameListTop + i * kListStride;
                const bool current = state.selected_game == static_cast<std::size_t>(index);
                rectangle(x, y, 480, kRowHeight, current ? colors.panelHover : colors.panel);
                if (current) rectangle(x, y, 4, kRowHeight, colors.accent);
                text(x + 17, y + 22, displayLabel(game.name, 42),
                     current ? colors.primary : colors.subdued, normal);
                text(x + 17, y + 43, displayLabel(game.subtitle, 63), colors.subdued, small);
            }
            if (static_cast<int>(entries.size()) > maxVisibleGames()) {
                text(x + 13, height - 120,
                     std::to_string(state.selected_game + 1) + "/" + std::to_string(entries.size()),
                     colors.accent, small);
            }
        }
        if (!gameStatus.empty()) {
            const auto status = displayLabel(gameStatus, 76);
            centered(width / 2, height - 94, status, colors.accent, small);
        }
        centered(width / 2, height - 65, "ENTER - SELECT  /  FLATPAK LAUNCH ONLY    R - REFRESH    ESC - BACK",
                 colors.subdued, small);
    }
    void diagnostics() const {
        const int x = origin_x();
        centered(width / 2, 82, "DIAGNOSTICS", colors.primary, heading);
        centered(width / 2, 109, "LIVE READ-ONLY SYSTEM INFORMATION", colors.accent, small);
        line(x, 132, x + 480, 132, colors.border);
        // Ignore newline styling from report_text; draw simple monospaced lines.
        constexpr int lineHeight = 19;
        const int top = 169;
        const int available = std::max(0, (height - 230) / lineHeight);
        const int lastStart = std::max(0, static_cast<int>(report.size()) - available);
        const int first = std::min(state.scroll_lines, lastStart);
        for (int i = 0; i < available && first + i < static_cast<int>(report.size()); ++i) {
            std::string lineText = report[first + i];
            if (lineText.size() > 76) lineText = lineText.substr(0, 73) + "...";
            text(x, top + i * lineHeight, lineText, colors.primary, small);
        }
        centered(width / 2, height - 70, "UP / DOWN  -  SCROLL       ESC  -  BACK", colors.subdued, small);
    }
    void draw() const {
        frame();
        if (state.screen == MenuScreen::Home) home();
        else if (state.screen == MenuScreen::Games) games();
        else diagnostics();
        XFlush(display);
    }
    bool hitItem(int mouseX, int mouseY, int& index) const {
        if (state.screen == MenuScreen::Diagnostics) return false;
        const int x = origin_x();
        const int y = state.screen == MenuScreen::Home ? item_y() : kGameListTop;
        const int count = state.screen == MenuScreen::Home ? kHomeActions :
            std::min(maxVisibleGames(), static_cast<int>(entries.size()) - firstVisibleGame());
        for (int i = 0; i < count; ++i) {
            const int top = y + i * kListStride;
            if (mouseX >= x && mouseX <= x + 480 && mouseY >= top && mouseY <= top + kRowHeight) {
                index = state.screen == MenuScreen::Games ? i + firstVisibleGame() : i;
                return true;
            }
        }
        return false;
    }
    void selectIndex(int index) {
        if (state.screen == MenuScreen::Home && index >= 0 && index < kHomeActions)
            state.selected = static_cast<MenuAction>(index);
        else if (state.screen == MenuScreen::Games && index >= 0 &&
                 static_cast<std::size_t>(index) < entries.size())
            state.selected_game = static_cast<std::size_t>(index);
    }
    void scroll(int direction) {
        if (state.screen != MenuScreen::Diagnostics) { menu_move(state, direction); return; }
        const int available = std::max(0, (height - 230) / 19);
        const int maxStart = std::max(0, static_cast<int>(report.size()) - available);
        state.scroll_lines = std::clamp(state.scroll_lines + direction, 0, maxStart);
    }
};

std::vector<std::string> splitLines(const std::string& value) {
    std::istringstream stream(value);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

XFontStruct* loadFont(Display* d, const char* preferred) {
    auto* font = XLoadQueryFont(d, preferred);
    return font ? font : XLoadQueryFont(d, "fixed");
}
}

int run_menu() {
    App app;
    app.display = XOpenDisplay(nullptr);
    if (!app.display) {
        std::cerr << "Cannot open X11 display. AstraPS5's native menu is an X11 prototype.\n"
                     "Run it in an X11/XWayland session with DISPLAY set.\n";
        return 1;
    }
    const int screen = DefaultScreen(app.display);
    app.colors = {
        rgb(app.display, "#0b0d17"), rgb(app.display, "#171b2b"),
        rgb(app.display, "#262139"), rgb(app.display, "#f3f1ff"),
        rgb(app.display, "#9299b5"), rgb(app.display, "#ae91ff"),
        rgb(app.display, "#33374a")
    };
    app.window = XCreateSimpleWindow(app.display, RootWindow(app.display, screen),
                                     80, 80, app.width, app.height, 0,
                                     app.colors.background, app.colors.background);
    XStoreName(app.display, app.window, "AstraPS5 - Experimental Menu");
    XSizeHints hints{};
    hints.flags = PMinSize;
    hints.min_width = kMinWidth;
    hints.min_height = kMinHeight;
    XSetWMNormalHints(app.display, app.window, &hints);
    app.deleteWindow = XInternAtom(app.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(app.display, app.window, &app.deleteWindow, 1);
    XSelectInput(app.display, app.window,
                 ExposureMask | StructureNotifyMask | KeyPressMask | ButtonPressMask | PointerMotionMask);
    app.gc = XCreateGC(app.display, app.window, 0, nullptr);
    app.heading = loadFont(app.display, "-adobe-helvetica-bold-r-normal--34-*-*-*-*-*-iso8859-1");
    app.normal = loadFont(app.display, "-adobe-helvetica-medium-r-normal--18-*-*-*-*-*-iso8859-1");
    app.small = loadFont(app.display, "-adobe-helvetica-medium-r-normal--13-*-*-*-*-*-iso8859-1");
    app.report = splitLines(report_text(collect_diagnostics()));
    app.refreshLibrary();
    const auto steamStatus = splitLines(steam_discovery_text(app.steam));
    app.report.emplace_back("");
    app.report.insert(app.report.end(), steamStatus.begin(), steamStatus.end());
    XMapWindow(app.display, app.window);
    while (app.state.running) {
        app.checkGame();
        if (!XPending(app.display)) {
            pollfd connection{ConnectionNumber(app.display), POLLIN, 0};
            const int result = ::poll(&connection, 1, 125);
            if (result < 0 && errno != EINTR) {
                std::cerr << "X11 event polling failed.\n";
                break;
            }
            continue;
        }
        XEvent event{};
        XNextEvent(app.display, &event);
        switch (event.type) {
            case Expose:
                if (event.xexpose.count == 0) app.draw();
                break;
            case ConfigureNotify:
                app.width = event.xconfigure.width;
                app.height = event.xconfigure.height;
                app.draw();
                break;
            case KeyPress: {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_Escape || key == XK_BackSpace) menu_back(app.state);
                else if (key == XK_Up || key == XK_k) app.scroll(-1);
                else if (key == XK_Down || key == XK_j) app.scroll(1);
                else if (key == XK_Return || key == XK_KP_Enter || key == XK_space) {
                    menu_select(app.state);
                    app.requestLaunch();
                }
                if (app.state.screen == MenuScreen::Games && (key == XK_r || key == XK_R))
                    app.refreshLibrary();
                if (app.state.screen == MenuScreen::Diagnostics) {
                    if (key == XK_Page_Down) app.scroll(10);
                    if (key == XK_Page_Up) app.scroll(-10);
                }
                app.draw();
                break;
            }
            case MotionNotify: {
                int index = 0;
                if (app.hitItem(event.xmotion.x, event.xmotion.y, index)) {
                    const auto previous = app.state.selected;
                    const auto previousGame = app.state.selected_game;
                    app.selectIndex(index);
                    if (previous != app.state.selected || previousGame != app.state.selected_game) app.draw();
                }
                break;
            }
            case ButtonPress: {
                if (event.xbutton.button == Button4) app.scroll(-1);
                else if (event.xbutton.button == Button5) app.scroll(1);
                else if (event.xbutton.button == Button1) {
                    int index = 0;
                    if (app.hitItem(event.xbutton.x, event.xbutton.y, index)) {
                        app.selectIndex(index);
                        menu_select(app.state);
                        app.requestLaunch();
                    }
                }
                app.draw();
                break;
            }
            case ClientMessage:
                if (static_cast<Atom>(event.xclient.data.l[0]) == app.deleteWindow)
                    app.state.running = false;
                break;
            default: break;
        }
    }
    if (app.heading) XFreeFont(app.display, app.heading);
    if (app.normal) XFreeFont(app.display, app.normal);
    if (app.small) XFreeFont(app.display, app.small);
    XFreeGC(app.display, app.gc);
    XDestroyWindow(app.display, app.window);
    XCloseDisplay(app.display);
    return 0;
}
} // namespace astra
