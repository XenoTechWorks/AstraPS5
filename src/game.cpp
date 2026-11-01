// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/game.hpp"
#include "astraps5/diagnostics.hpp"

#include <array>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <spawn.h>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char** environ;
namespace fs = std::filesystem;
namespace astra {
namespace {

std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r");
    if (begin == std::string::npos) return {};
    const auto end = s.find_last_not_of(" \t\r");
    return s.substr(begin, end - begin + 1);
}

bool has_control_char(const std::string& value) {
    for (unsigned char ch : value) if (ch < 32 || ch == 127) return true;
    return false;
}

bool executable_file(const fs::path& candidate) {
    std::error_code ec;
    return fs::is_regular_file(candidate, ec) && ::access(candidate.c_str(), X_OK) == 0;
}

fs::path resolve_command(const std::string& command, const fs::path& manifest) {
    const fs::path path(command);
    if (path.is_absolute()) return executable_file(path) ? path : fs::path{};
    if (command.find('/') != std::string::npos) {
        const fs::path result = fs::absolute(manifest).parent_path() / path;
        return executable_file(result) ? result.lexically_normal() : fs::path{};
    }
    const char* rawPath = std::getenv("PATH");
    const std::string pathEnv = rawPath ? rawPath : "/usr/local/bin:/usr/bin:/bin";
    std::size_t start = 0;
    for (;;) {
        const auto end = pathEnv.find(':', start);
        auto dir = pathEnv.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (dir.empty()) dir = ".";
        const fs::path candidate = fs::path(dir) / command;
        if (executable_file(candidate)) return fs::absolute(candidate).lexically_normal();
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

bool parse_arg_index(const std::string& key, int& index) {
    if (key.rfind("arg", 0) != 0 || key.size() < 4 || key.size() > 5) return false;
    const auto digits = key.substr(3);
    for (unsigned char c : digits) if (!std::isdigit(c)) return false;
    if (digits.size() > 1 && digits[0] == '0') return false;
    index = std::stoi(digits);
    return index >= 0 && index < 16;
}

bool valid_app_id(const std::string& id) {
    if (id.size() < 5 || id.size() > 190 || id.front() == '.' || id.back() == '.' ||
        id.find('.') == std::string::npos || id.find("..") != std::string::npos) return false;
    for (unsigned char ch : id) {
        if (!std::isalnum(ch) && ch != '-' && ch != '_' && ch != '.') return false;
    }
    return true;
}

bool command_succeeded(const fs::path& program, const std::vector<std::string>& arguments) {
    // Child output goes to /dev/null: probing Flatpak must not clutter the CLI.
    posix_spawn_file_actions_t actions{};
    if (posix_spawn_file_actions_init(&actions) != 0) return false;
    int result = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    if (result == 0) result = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    if (result != 0) { posix_spawn_file_actions_destroy(&actions); return false; }
    std::vector<std::string> strings{program.string()};
    strings.insert(strings.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (auto& s : strings) argv.push_back(s.data());
    argv.push_back(nullptr);
    pid_t child = -1;
    result = posix_spawn(&child, program.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (result != 0) return false;
    int status = 0;
    while (::waitpid(child, &status, 0) == -1) {
        if (errno == EINTR) continue;
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

GameProfile inspect_flatpak_impl(const std::string& name, const std::string& app_id,
                                const fs::path& manifest_path) {
    GameProfile result;
    result.name = name;
    result.runtime = GameRuntime::Flatpak;
    result.flatpak_id = app_id;
    result.manifest_path = manifest_path;
    if (name.empty() || name.size() > 96 || has_control_char(name)) {
        result.error = "Game name is missing, invalid or too long.";
        return result;
    }
    if (!valid_app_id(app_id)) {
        result.error = "Invalid Flatpak application ID.";
        return result;
    }
    result.executable_path = resolve_command("flatpak", manifest_path);
    if (result.executable_path.empty()) {
        result.error = "Flatpak executable not found in PATH.";
        return result;
    }
    if (!command_succeeded(result.executable_path, {"info", "--show-ref", app_id})) {
        result.error = "Flatpak application not installed or unavailable: " + app_id;
        return result;
    }
    result.valid = true;
    return result;
}

std::vector<std::string> process_arguments(const GameProfile& profile) {
    if (profile.runtime == GameRuntime::Flatpak) {
        std::vector<std::string> result{"run", profile.flatpak_id};
        result.insert(result.end(), profile.arguments.begin(), profile.arguments.end());
        return result;
    }
    return profile.arguments;
}

// Each launch has its own log, even when a game is started twice.
fs::path state_log_dir() {
    const char* xdg = std::getenv("XDG_STATE_HOME");
    if (xdg && *xdg && fs::path(xdg).is_absolute()) return fs::path(xdg) / "astraps5" / "logs";
    const char* home = std::getenv("HOME");
    if (!home || !*home || !fs::path(home).is_absolute()) return {};
    return fs::path(home) / ".local" / "state" / "astraps5" / "logs";
}

GameProcessStatus status_from_wait(int status) {
    if (WIFEXITED(status)) {
        const int code = WEXITSTATUS(status);
        return {GameProcessState::Exited, code,
                "Game exited with code " + std::to_string(code) + "."};
    }
    if (WIFSIGNALED(status)) {
        const int signal = WTERMSIG(status);
        return {GameProcessState::Signaled, 128 + signal,
                "Game terminated by signal " + std::to_string(signal) + "."};
    }
    return {GameProcessState::Error, 1, "Unexpected process state."};
}

} // namespace

GameProfile inspect_flatpak_app(const std::string& name, const std::string& app_id) {
    return inspect_flatpak_impl(name, app_id, fs::current_path() / "builtin-flatpak.game");
}

GameProfile inspect_game_profile(const fs::path& manifest_path) {
    GameProfile result;
    result.manifest_path = manifest_path;
    std::error_code ec;
    if (!fs::is_regular_file(manifest_path, ec)) {
        result.error = "Game profile not found (or not a regular file).";
        return result;
    }
    const auto bytes = fs::file_size(manifest_path, ec);
    if (ec || bytes > 16384) {
        result.error = "Game profile exceeds the 16 KiB size limit.";
        return result;
    }
    std::ifstream input(manifest_path);
    if (!input) { result.error = "Could not read game profile."; return result; }
    std::map<std::string, std::string> fields;
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        auto current = trim(line);
        if (current.empty() || current[0] == '#') continue;
        const auto equals = current.find('=');
        if (equals == std::string::npos) {
            result.error = "Expected key=value on line " + std::to_string(lineNumber) + ".";
            return result;
        }
        const std::string key = trim(current.substr(0, equals));
        const std::string value = trim(current.substr(equals + 1));
        int argIndex = -1;
        if (key != "format" && key != "name" && key != "executable" && key != "runtime" &&
            key != "app_id" && !parse_arg_index(key, argIndex)) {
            result.error = "Unsupported game profile key: " + key;
            return result;
        }
        if (fields.count(key)) {
            result.error = "Duplicate game profile key: " + key;
            return result;
        }
        if (value.size() > 2048 || has_control_char(value)) {
            result.error = "Invalid or oversized game profile value on line " + std::to_string(lineNumber) + ".";
            return result;
        }
        fields.emplace(key, value);
    }
    if (input.bad()) { result.error = "Failed while reading game profile."; return result; }
    if (fields["format"] != "astra-game-v1") {
        result.error = "Unsupported game profile format (expected astra-game-v1).";
        return result;
    }
    result.name = fields["name"];
    if (result.name.empty() || result.name.size() > 96 || has_control_char(result.name)) {
        result.error = "Game name is missing, invalid or too long.";
        return result;
    }
    const std::string runtime = fields.count("runtime") ? fields.at("runtime") : "native";
    if (runtime != "native" && runtime != "flatpak") {
        result.error = "Unsupported game runtime (expected native or flatpak).";
        return result;
    }
    for (int i = 0; i < 16; ++i) {
        const auto it = fields.find("arg" + std::to_string(i));
        if (it == fields.end()) {
            for (int j = i + 1; j < 16; ++j) {
                if (fields.count("arg" + std::to_string(j))) {
                    result.error = "Argument indexes must start at arg0 and be contiguous.";
                    return result;
                }
            }
            break;
        }
        result.arguments.push_back(it->second);
    }
    if (runtime == "flatpak") {
        if (fields.count("executable")) {
            result.error = "Flatpak profiles must use app_id, not executable.";
            return result;
        }
        auto checked = inspect_flatpak_impl(result.name, fields["app_id"], manifest_path);
        checked.arguments = result.arguments;
        return checked;
    }
    if (fields.count("app_id")) {
        result.error = "Native profiles cannot define app_id.";
        return result;
    }
    const std::string command = fields["executable"];
    if (command.empty()) { result.error = "Missing executable in game profile."; return result; }
    result.executable_path = resolve_command(command, manifest_path);
    if (result.executable_path.empty()) {
        result.error = "Executable missing or not executable: " + command;
        return result;
    }
    result.valid = true;
    return result;
}

std::string game_profile_text(const GameProfile& profile) {
    std::string out = "AstraPS5 Linux game preflight\n";
    out += "  Profile: " + profile.manifest_path.string() + "\n";
    out += "  Game:    " + profile.name + "\n";
    out += "  Runtime: " + std::string(profile.runtime == GameRuntime::Flatpak ? "Flatpak" : "native") + "\n";
    if (!profile.valid) {
        out += "  Status:  NOT READY\n  Reason:  " + profile.error + "\n";
    } else {
        out += "  Status:  READY (Linux PC only)\n";
        out += "  Program: " + profile.executable_path.string() + "\n";
        if (profile.runtime == GameRuntime::Flatpak) out += "  App ID:  " + profile.flatpak_id + "\n";
        out += "  Args:    " + std::to_string(profile.arguments.size()) + "\n";
    }
    out += "  PS5 compatibility: NOT TESTED\n";
    return out;
}

std::string game_profile_json(const GameProfile& profile) {
    return "{\n"
           "  \"ready\": " + std::string(profile.valid ? "true" : "false") + ",\n"
           "  \"name\": \"" + escape_json(profile.name) + "\",\n"
           "  \"runtime\": \"" + std::string(profile.runtime == GameRuntime::Flatpak ? "flatpak" : "native") + "\",\n"
           "  \"app_id\": \"" + escape_json(profile.flatpak_id) + "\",\n"
           "  \"profile\": \"" + escape_json(profile.manifest_path.string()) + "\",\n"
           "  \"executable\": \"" + escape_json(profile.executable_path.string()) + "\",\n"
           "  \"argument_count\": " + std::to_string(profile.arguments.size()) + ",\n"
           "  \"error\": \"" + escape_json(profile.error) + "\",\n"
           "  \"ps5_tested\": false\n"
           "}\n";
}

GameLaunch start_game(const GameProfile& profile) {
    GameLaunch launch;
    if (!profile.valid || profile.executable_path.empty()) {
        launch.error = "Game profile has not passed validation.";
        return launch;
    }
    const auto dir = state_log_dir();
    if (dir.empty()) {
        launch.error = "Set an absolute HOME or XDG_STATE_HOME to store launch logs.";
        return launch;
    }
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) { launch.error = "Cannot create log directory: " + ec.message(); return launch; }
    std::string name = profile.name.substr(0, 45);
    for (char& ch : name) if (!std::isalnum(static_cast<unsigned char>(ch))) ch = '-';
    std::string pattern = (dir / (name + "-XXXXXX.log")).string();
    std::vector<char> temp(pattern.begin(), pattern.end());
    temp.push_back('\0');
    const int log_fd = ::mkstemps(temp.data(), 4);
    if (log_fd < 0) {
        launch.error = std::string("Cannot open game log: ") + std::strerror(errno);
        return launch;
    }
    launch.log_path = fs::path(temp.data());
    ::fchmod(log_fd, S_IRUSR | S_IWUSR);
    posix_spawn_file_actions_t actions{};
    const int init_result = posix_spawn_file_actions_init(&actions);
    int result = init_result;
    if (result == 0) result = posix_spawn_file_actions_adddup2(&actions, log_fd, STDOUT_FILENO);
    if (result == 0) result = posix_spawn_file_actions_adddup2(&actions, log_fd, STDERR_FILENO);
    if (result == 0) result = posix_spawn_file_actions_addclose(&actions, log_fd);
    if (result != 0) {
        if (init_result == 0) posix_spawn_file_actions_destroy(&actions);
        ::close(log_fd);
        fs::remove(launch.log_path, ec);
        launch.log_path.clear();
        launch.error = std::string("Cannot configure log capture: ") + std::strerror(result);
        return launch;
    }
    std::vector<std::string> strings{profile.executable_path.string()};
    auto args = process_arguments(profile);
    strings.insert(strings.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (auto& value : strings) argv.push_back(value.data());
    argv.push_back(nullptr);
    pid_t child = -1;
    result = posix_spawn(&child, profile.executable_path.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    ::close(log_fd);
    if (result != 0) {
        fs::remove(launch.log_path, ec);
        launch.log_path.clear();
        launch.error = std::string("Cannot start game: ") + std::strerror(result);
        return launch;
    }
    launch.pid = child;
    return launch;
}

GameProcessStatus poll_game(pid_t pid) {
    if (pid <= 0) return {GameProcessState::Error, 1, "Invalid game process ID."};
    int status = 0;
    pid_t result;
    do { result = ::waitpid(pid, &status, WNOHANG); } while (result < 0 && errno == EINTR);
    if (result == 0) return {GameProcessState::Running, 0, "Game is running."};
    if (result < 0) return {GameProcessState::Error, 1, "Cannot wait for game process: " + std::string(std::strerror(errno))};
    return status_from_wait(status);
}

GameProcessStatus wait_game(pid_t pid) {
    if (pid <= 0) return {GameProcessState::Error, 1, "Invalid game process ID."};
    int status = 0;
    pid_t result;
    do { result = ::waitpid(pid, &status, 0); } while (result < 0 && errno == EINTR);
    if (result < 0) return {GameProcessState::Error, 1, "Cannot wait for game process: " + std::string(std::strerror(errno))};
    return status_from_wait(status);
}

int launch_game(const GameProfile& profile) {
    const auto game = start_game(profile);
    if (!game.started()) { std::cerr << "Game launch failed: " << game.error << "\n"; return 1; }
    std::cout << "Game started (PID " << game.pid << "). Log: " << game.log_path << "\n" << std::flush;
    const auto result = wait_game(game.pid);
    std::cout << result.message << " Log: " << game.log_path << "\n";
    return result.exit_code;
}

} // namespace astra
