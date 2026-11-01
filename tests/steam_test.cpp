// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/steam.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
namespace {
void write(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path);
    assert(stream.good());
    stream << data;
    assert(stream.good());
}
std::string manifest(const std::string& id, const std::string& name,
                     const std::string& installDir, const std::string& flags = "4") {
    return "// Game install metadata\n\"AppState\"\n{\n\"appid\" \"" + id +
           "\"\n\"name\" \"" + name + "\"\n\"installdir\" \"" + installDir +
           "\"\n\"StateFlags\" \"" + flags + "\"\n}\n";
}
}
int main() {
    char buffer[] = "/tmp/astra-steam-XXXXXX";
    const char* temp = ::mkdtemp(buffer);
    assert(temp);
    const fs::path root(temp);
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{root};
    const auto main = root / "Steam";
    const auto secondary = root / "External Drive/SteamLibrary";
    fs::create_directories(main / "steamapps/common/Example Game");
    fs::create_directories(secondary / "steamapps/common/Secondary Game");
    fs::create_directories(main / "steamapps/common/Proton - Experimental");
    fs::create_directories(main / "compatibilitytools.d/GE-Proton10-15");
    write(main / "steamapps/common/Proton - Experimental/proton", "#!/bin/sh\n");
    write(main / "compatibilitytools.d/GE-Proton10-15/proton", "#!/bin/sh\n");
    write(main / "compatibilitytools.d/GE-Proton10-15/compatibilitytool.vdf", "\"compatibilitytools\" {}\n");
    write(main / "steamapps/libraryfolders.vdf",
          "// Version 2 libraryfolders example\n\"libraryfolders\"\n{\n\"0\" { \"path\" \"" +
          main.string() + "\" \"apps\" {\"111\" \"900\"} }\n\"1\" { \"path\" \"" +
          secondary.string() + "\" \"mounted\" \"1\" }\n}\n");
    write(main / "steamapps/appmanifest_111.acf", manifest("111", "Example Game", "Example Game"));
    write(secondary / "steamapps/appmanifest_222.acf", manifest("222", "Secondary Game", "Secondary Game"));
    write(main / "steamapps/appmanifest_333.acf", manifest("333", "Half Installed", "Missing", "2"));
    write(main / "steamapps/appmanifest_444.acf", manifest("444", "Escape", "../Private"));
    write(main / "steamapps/appmanifest_555.acf", manifest("555", "Stale", "Missing"));
    write(main / "steamapps/appmanifest_666.acf", manifest("999", "Wrong ID", "Example Game"));
    write(main / "steamapps/appmanifest_777.acf", manifest("777", "Proton - Experimental", "Proton - Experimental"));
    write(main / "steamapps/appmanifest_888.acf", "\"AppState\" { \"name\" \"broken\"\n");

    const auto result = astra::discover_steam_in({main, main / ".", root / "missing"});
    assert(result.steam_directory_found);
    assert(result.libraries.size() == 2);
    assert(result.games.size() == 2);
    assert(result.games[0].app_id == "111" && result.games[0].name == "Example Game");
    assert(result.games[1].app_id == "222" && result.games[1].name == "Secondary Game");
    assert(result.protons.size() == 2);
    assert(result.protons[0].custom != result.protons[1].custom);
    const auto text = astra::steam_discovery_text(result);
    assert(text.find("Example Game") != std::string::npos);
    assert(text.find("NOT TESTED") != std::string::npos);
    const auto json = astra::steam_discovery_json(result);
    assert(json.find("\"game_count\": 2") != std::string::npos);
    assert(json.find("\"proton_count\": 2") != std::string::npos);
    assert(json.find("\"ps5_tested\": false") != std::string::npos);
    const auto empty = astra::discover_steam_in({root / "no-steam"});
    assert(!empty.steam_directory_found && empty.games.empty() && empty.protons.empty());

    // Old-style numbered library entries also work.
    write(main / "config/libraryfolders.vdf",
          "\"LibraryFolders\" { \"0\" \"" + secondary.string() + "\" }\n");
    const auto legacy = astra::discover_steam_in({main});
    assert(legacy.libraries.size() == 2 && legacy.games.size() == 2);
    std::cout << "Steam library / Proton discovery, stale manifest rejection and JSON tests passed.\n";
}
