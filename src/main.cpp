// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/boot.hpp"
#include "astraps5/game.hpp"
#include "astraps5/library.hpp"
#include "astraps5/steam.hpp"
#include "astraps5/diagnostics.hpp"
#include "astraps5/menu.hpp"
#include <iostream>
#include <string>

namespace {
void help() {
    std::cout << "AstraPS5 0.2.5-dev - Experimental Test Project\n\n"
                 "Usage:\n"
                 "  astra menu               Open the minimal native Astra UI\n"
                 "  astra doctor [--json]    Read-only Linux and gaming diagnostics\n"
                 "  astra boot verify <manifest> [--json]  Verify Linux image integrity (no boot)\n"
                 "  astra games list [--json]  Discover installed Flatpak games\n"
                 "  astra games run <app_id>  Launch a detected Flatpak game\n"
                 "  astra steam list [--json]  Discover installed Steam titles / Proton builds\n"
                 "  astra game check <profile> [--json]   Check native Linux game profile\n"
                 "  astra game run <profile>   Launch native Linux executable (explicit)\n"
                 "  astra version            Print current version\n"
                 "  astra help               Show this help\n\n"
                 "PS5 booting, Steam launching, Proton execution, and multiplayer are NOT implemented.\n";
}
}

int main(int argc, char* argv[]) {
    const std::string command = argc > 1 ? argv[1] : "help";
    if (command == "help" || command == "--help" || command == "-h") {
        if (argc != 2 && argc != 1) { std::cerr << "Unexpected argument.\n"; return 2; }
        help();
        return 0;
    }
    if (command == "version" && argc == 2) {
        std::cout << "AstraPS5 0.2.5-dev\n";
        return 0;
    }
    if (command == "menu" && argc == 2) {
        return astra::run_menu();
    }
    if (command == "boot" && argc >= 3 && std::string(argv[2]) == "verify" &&
        (argc == 4 || (argc == 5 && std::string(argv[4]) == "--json"))) {
        const auto result = astra::verify_boot_manifest(argv[3]);
        std::cout << (argc == 5 ? astra::boot_verification_json(result)
                               : astra::boot_verification_text(result));
        return result.verified ? 0 : 1;
    }
    if (command == "steam" && argc >= 3 && std::string(argv[2]) == "list" &&
        (argc == 3 || (argc == 4 && std::string(argv[3]) == "--json"))) {
        const auto discovery = astra::discover_steam();
        std::cout << (argc == 4 ? astra::steam_discovery_json(discovery)
                                : astra::steam_discovery_text(discovery));
        return 0;
    }
    if (command == "games" && argc >= 3) {
        const std::string subcommand = argv[2];
        if (subcommand == "list" && (argc == 3 || (argc == 4 && std::string(argv[3]) == "--json"))) {
            const auto library = astra::discover_flatpak_games();
            std::cout << (argc == 4 ? astra::game_library_json(library)
                                    : astra::game_library_text(library));
            return library.error.empty() ? 0 : 1;
        }
        if (subcommand == "run" && argc == 4) {
            const auto library = astra::discover_flatpak_games();
            for (const auto& game : library.games) {
                if (game.flatpak_id == argv[3]) {
                    // Revalidate because the app could have been uninstalled.
                    const auto verified = astra::inspect_flatpak_app(game.name, game.flatpak_id);
                    if (!verified.valid) { std::cerr << verified.error << '\n'; return 1; }
                    return astra::launch_game(verified);
                }
            }
            std::cerr << "Game is not in the discovered Flatpak game library: " << argv[3] << '\n';
            return 1;
        }
    }
    if (command == "game" && (argc == 4 || (argc == 5 && std::string(argv[4]) == "--json"))) {
        const std::string mode = argv[2];
        if (mode == "check" || (mode == "run" && argc == 4)) {
            const auto profile = astra::inspect_game_profile(argv[3]);
            if (mode == "check") {
                std::cout << (argc == 5 ? astra::game_profile_json(profile)
                                       : astra::game_profile_text(profile));
                return profile.valid ? 0 : 1;
            }
            if (!profile.valid) {
                std::cerr << astra::game_profile_text(profile);
                return 1;
            }
            std::cout << "Launching Linux game: " << profile.name << "\n" << std::flush;
            return astra::launch_game(profile);
        }
    }
    if (command == "doctor" && (argc == 2 || (argc == 3 && std::string(argv[2]) == "--json"))) {
        const auto report = astra::collect_diagnostics();
        std::cout << (argc == 3 ? astra::report_json(report) : astra::report_text(report));
        return 0;
    }
    std::cerr << "Unknown command or argument. Use: astra help\n";
    return 2;
}
