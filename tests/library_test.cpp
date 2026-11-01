// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/library.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
int main() {
    char tmp[] = "/tmp/astra-library-XXXXXX";
    const char* dir = mkdtemp(tmp);
    assert(dir != nullptr);
    const fs::path root(dir);
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{root};
    const fs::path bin = root / "bin";
    const fs::path user = root / "user/applications";
    const fs::path system = root / "system/applications";
    fs::create_directories(bin);
    fs::create_directories(user);
    fs::create_directories(system);
    const fs::path fake = bin / "flatpak";
    {
        std::ofstream out(fake);
        out << "#!/bin/sh\n"
               "if [ \"$1\" = info ] && [ \"$2\" = --show-ref ]; then\n"
               " case \"$3\" in net.supertuxkart.SuperTuxKart|org.example.Puzzle) exit 0;; esac\n"
               "fi\nexit 1\n";
    }
    fs::permissions(fake, fs::perms::owner_all);
    const std::string original = std::getenv("PATH") ? std::getenv("PATH") : "/usr/bin:/bin";
    assert(setenv("PATH", (bin.string() + ":" + original).c_str(), 1) == 0);
    auto write = [](const fs::path& path, const std::string& contents) {
        std::ofstream out(path);
        assert(out.good());
        out << contents;
    };
    const std::string prefix = "[Desktop Entry]\nType=Application\n";
    // Whitelist by Categories=Game and successful flatpak info --show-ref.
    write(user / "net.supertuxkart.SuperTuxKart.desktop",
          prefix + "Name=SuperTuxKart\nCategories=Game;ArcadeGame;\n"
          "Exec=sh -c 'touch /tmp/should-never-run'\n");
    write(system / "net.supertuxkart.SuperTuxKart.desktop",
          prefix + "Name=Wrong duplicate\nCategories=Game;\n");
    write(system / "org.example.Puzzle.desktop",
          prefix + "Name=Puzzle Time\nCategories=Game;LogicGame;\n");
    write(user / "org.example.Note.desktop",
          prefix + "Name=Notes\nCategories=Office;\n");
    write(user / "org.example.Stale.desktop",
          prefix + "Name=Stale\nCategories=Game;\n");
    write(user / "org.example.Hidden.desktop",
          prefix + "Name=Hidden\nCategories=Game;\nHidden=true\n");
    write(user / "org.example.Tool.desktop",
          prefix + "Name=Game utility\nCategories=GameUtility;\n");
    write(user / "org.example.Invalid.desktop",
          prefix + "Name=Not a game\nCategories=Game;\nType=Link\n");
    write(user / "org.example.Control.desktop",
          prefix + "Name=Break\\nLine\nCategories=Game;\n");
    auto games = astra::discover_flatpak_games_in({user, system});
    assert(games.error.empty() && games.games.size() == 2);
    assert(games.games[0].name == "Puzzle Time");
    assert(games.games[0].flatpak_id == "org.example.Puzzle");
    assert(games.games[1].name == "SuperTuxKart");
    assert(games.games[1].flatpak_id == "net.supertuxkart.SuperTuxKart");
    assert(astra::game_library_text(games).find("SuperTuxKart") != std::string::npos);
    assert(astra::game_library_json(games).find("\"count\": 2") != std::string::npos);
    assert(astra::game_library_json(games).find("\"ps5_tested\": false") != std::string::npos);
    auto empty = astra::discover_flatpak_games_in({root / "nope"});
    assert(empty.games.empty() && empty.error.empty());
    assert(astra::game_library_json(empty).find("\"games\": [") != std::string::npos);
    assert(setenv("PATH", original.c_str(), 1) == 0);
    std::cout << "Flatpak game discovery filters, deduplication and JSON tests passed.\n";
}
