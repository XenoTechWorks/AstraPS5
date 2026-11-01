// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/game.hpp"
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
    char dir[] = "/tmp/astra-game-test-XXXXXX";
    const char* temp = mkdtemp(dir);
    assert(temp != nullptr);
    const fs::path root(temp);
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{root};
    assert(setenv("XDG_STATE_HOME", (root / "state").c_str(), 1) == 0);
    const fs::path profile = root / "test.game";
    const auto write = [&](const std::string& body) {
        std::ofstream f(profile);
        assert(f.good());
        f << body;
    };
    const std::string base = "# Test Linux game launcher\nformat=astra-game-v1\nname=Test Game\n";
    write(base + "executable=/bin/true\n");
    auto good = astra::inspect_game_profile(profile);
    assert(good.valid && good.name == "Test Game");
    assert(good.runtime == astra::GameRuntime::Native);
    assert(good.arguments.empty());
    assert(astra::launch_game(good) == 0);
    assert(astra::game_profile_json(good).find("\"ready\": true") != std::string::npos);
    assert(astra::game_profile_text(good).find("PS5 compatibility: NOT TESTED") != std::string::npos);

    write(base + "executable=/bin/false\n");
    good = astra::inspect_game_profile(profile);
    assert(good.valid && astra::launch_game(good) == 1);

    write(base + "executable=/bin/echo\narg0=Hello game log\narg1=some literal; echo NOT_EXECUTED\n");
    good = astra::inspect_game_profile(profile);
    assert(good.valid && good.arguments.size() == 2);
    const auto running = astra::start_game(good);
    assert(running.started() && !running.log_path.empty());
    const auto outcome = astra::wait_game(running.pid);
    assert(outcome.state == astra::GameProcessState::Exited && outcome.exit_code == 0);
    std::ifstream log(running.log_path);
    assert(log.good());
    std::string contents((std::istreambuf_iterator<char>(log)), std::istreambuf_iterator<char>());
    assert(contents.find("Hello game log") != std::string::npos);
    assert(contents.find("some literal; echo NOT_EXECUTED") != std::string::npos);
    assert(!astra::poll_game(running.pid).message.empty()); // cannot reap a process twice

    write(base + "executable=/bin/true\narg1=missing-zero\n");
    assert(astra::inspect_game_profile(profile).error.find("contiguous") != std::string::npos);
    write(base + "executable=/bin/true\nformat=astra-game-v1\n");
    assert(astra::inspect_game_profile(profile).error.find("Duplicate") != std::string::npos);
    write(base + "executable=/bin/true\nunsupported=value\n");
    assert(astra::inspect_game_profile(profile).error.find("Unsupported") != std::string::npos);
    write(base + "executable=/bin/does-not-exist\n");
    assert(astra::inspect_game_profile(profile).error.find("missing") != std::string::npos);
    write("format=astra-game-v0\nname=Test\nexecutable=/bin/true\n");
    assert(astra::inspect_game_profile(profile).error.find("format") != std::string::npos);
    write(base + "executable=/bin/true\nname=Again\n");
    assert(astra::inspect_game_profile(profile).error.find("Duplicate") != std::string::npos);
    write(base + "executable=/bin/true\narg0=hello\narg01=bad\n");
    assert(astra::inspect_game_profile(profile).error.find("Unsupported") != std::string::npos);
    assert(!astra::inspect_game_profile(root / "missing.game").valid);

    // Test first-class Flatpak support without installing any application.
    // The fake executable behaves like the documented Flatpak CLI and records
    // a successful run in the exact same way a real CLI tool would.
    const fs::path bin = root / "bin";
    fs::create_directories(bin);
    const fs::path fake = bin / "flatpak";
    {
        std::ofstream script(fake);
        script << "#!/bin/sh\n"
                  "if [ \"$1\" = info ] && [ \"$2\" = --show-ref ] && "
                  "[ \"$3\" = net.supertuxkart.SuperTuxKart ]; then exit 0; fi\n"
                  "if [ \"$1\" = run ] && [ \"$2\" = net.supertuxkart.SuperTuxKart ]; then "
                  "printf 'Mock Flatpak game ran\\n'; exit 0; fi\n"
                  "exit 1\n";
    }
    fs::permissions(fake, fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write);
    const std::string currentPath = std::getenv("PATH") ? std::getenv("PATH") : "/usr/bin:/bin";
    assert(setenv("PATH", (bin.string() + ":" + currentPath).c_str(), 1) == 0);
    write("format=astra-game-v1\nname=SuperTuxKart\nruntime=flatpak\napp_id=net.supertuxkart.SuperTuxKart\n");
    auto flatpak = astra::inspect_game_profile(profile);
    assert(flatpak.valid && flatpak.runtime == astra::GameRuntime::Flatpak);
    assert(flatpak.flatpak_id == "net.supertuxkart.SuperTuxKart");
    const auto spawn = astra::start_game(flatpak);
    assert(spawn.started());
    const auto finished = astra::wait_game(spawn.pid);
    assert(finished.exit_code == 0);
    std::ifstream flatpakLog(spawn.log_path);
    assert(flatpakLog.good());
    std::string gameText((std::istreambuf_iterator<char>(flatpakLog)), std::istreambuf_iterator<char>());
    assert(gameText.find("Mock Flatpak game ran") != std::string::npos);
    assert(astra::inspect_flatpak_app("SuperTuxKart", "net.supertuxkart.SuperTuxKart").valid);
    assert(!astra::inspect_flatpak_app("Bad", "../evil").valid);
    write("format=astra-game-v1\nname=SuperTuxKart\nruntime=flatpak\napp_id=not.installed.Application\n");
    assert(!astra::inspect_game_profile(profile).valid);
    write(base + "runtime=flatpak\nexecutable=/bin/true\napp_id=net.supertuxkart.SuperTuxKart\n");
    assert(astra::inspect_game_profile(profile).error.find("app_id") != std::string::npos);
    write(base + "runtime=native\nexecutable=/bin/true\napp_id=net.supertuxkart.SuperTuxKart\n");
    assert(astra::inspect_game_profile(profile).error.find("cannot define app_id") != std::string::npos);
    write(base + "runtime=docker\nexecutable=/bin/true\n");
    assert(astra::inspect_game_profile(profile).error.find("Unsupported game runtime") != std::string::npos);
    assert(setenv("PATH", currentPath.c_str(), 1) == 0);
    std::cout << "Native and Flatpak profiles, process exit, and log capture tests passed.\n";
}
