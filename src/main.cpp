// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/boot.hpp"
#include "astraps5/diagnostics.hpp"
#include "astraps5/menu.hpp"
#include <iostream>
#include <string>

namespace {
void help() {
    std::cout << "AstraPS5 0.2.1-dev - Experimental Test Project\n\n"
                 "Usage:\n"
                 "  astra menu               Open the minimal native Astra UI\n"
                 "  astra doctor [--json]    Read-only Linux and gaming diagnostics\n"
                 "  astra boot verify <manifest> [--json]  Verify Linux image integrity (no boot)\n"
                 "  astra version            Print current version\n"
                 "  astra help               Show this help\n\n"
                 "AstraPS5 does not currently launch games or access PSN.\n";
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
        std::cout << "AstraPS5 0.2.1-dev\n";
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
    if (command == "doctor" && (argc == 2 || (argc == 3 && std::string(argv[2]) == "--json"))) {
        const auto report = astra::collect_diagnostics();
        std::cout << (argc == 3 ? astra::report_json(report) : astra::report_text(report));
        return 0;
    }
    std::cerr << "Unknown command or argument. Use: astra help\n";
    return 2;
}
