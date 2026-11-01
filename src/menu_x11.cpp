// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
// Minimal AstraPS5 native shell prototype. No game/boot actions are implemented.
#include "astraps5/menu.hpp"
#include "astraps5/menu_state.hpp"
#include "astraps5/diagnostics.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace astra {
namespace {
constexpr int kMinWidth = 540;
constexpr int kMinHeight = 440;
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
        centered(width / 2, height / 2 - 111, "PC GAMING ON PLAYSTATION HARDWARE", colors.subdued, small);
        line(width / 2 - 55, height / 2 - 87, width / 2 + 55, height / 2 - 87, colors.accent);
        const int x = origin_x(), y = item_y();
        const std::array<const char*, 2> labels = {"Diagnostics", "Exit"};
        for (int i = 0; i < 2; ++i) {
            bool current = (i == 0 && state.selected == MenuAction::Diagnostics)
                        || (i == 1 && state.selected == MenuAction::Exit);
            rectangle(x, y + i * (kRowHeight + 12), 480, kRowHeight,
                      current ? colors.panelHover : colors.panel);
            if (current) rectangle(x, y + i * (kRowHeight + 12), 4, kRowHeight, colors.accent);
            text(x + 24, y + 34 + i * (kRowHeight + 12), labels[i],
                 current ? colors.primary : colors.subdued, normal);
            if (current) text(x + 443, y + 34 + i * (kRowHeight + 12), ">", colors.accent, normal);
        }
        centered(width / 2, y + 173, "UP / DOWN  -  SELECT       ENTER  -  OPEN       ESC  -  QUIT",
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
        else diagnostics();
        XFlush(display);
    }
    bool hitItem(int mouseX, int mouseY, int& index) const {
        if (state.screen != MenuScreen::Home) return false;
        const int x = origin_x(), y = item_y();
        for (int i = 0; i < 2; ++i) {
            const int top = y + i * (kRowHeight + 12);
            if (mouseX >= x && mouseX <= x + 480 && mouseY >= top && mouseY <= top + kRowHeight) {
                index = i;
                return true;
            }
        }
        return false;
    }
    void selectIndex(int index) {
        state.selected = index == 0 ? MenuAction::Diagnostics : MenuAction::Exit;
    }
    void scroll(int direction) {
        if (state.screen == MenuScreen::Home) { menu_move(state, direction); return; }
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
    XMapWindow(app.display, app.window);
    while (app.state.running) {
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
                else if (key == XK_Return || key == XK_KP_Enter || key == XK_space) menu_select(app.state);
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
                    auto previous = app.state.selected;
                    app.selectIndex(index);
                    if (previous != app.state.selected) app.draw();
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
