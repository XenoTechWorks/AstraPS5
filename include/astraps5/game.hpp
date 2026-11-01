// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <filesystem>
#include <string>
#include <sys/types.h>
#include <vector>

namespace astra {

enum class GameRuntime { Native, Flatpak };

// This launcher runs Linux applications on the development host. It does not
// check or imply PS5 hardware compatibility, Steam integration, or multiplayer.
struct GameProfile {
    bool valid = false;
    std::string error;
    std::string name;
    GameRuntime runtime = GameRuntime::Native;
    std::string flatpak_id;
    std::filesystem::path manifest_path;
    std::filesystem::path executable_path;
    std::vector<std::string> arguments;
};

GameProfile inspect_game_profile(const std::filesystem::path& manifest_path);
// Built-in probe for the minimal menu; avoids profiles depending on the CWD.
GameProfile inspect_flatpak_app(const std::string& name, const std::string& app_id);
std::string game_profile_text(const GameProfile& profile);
std::string game_profile_json(const GameProfile& profile);

struct GameLaunch {
    pid_t pid = -1;
    std::filesystem::path log_path;
    std::string error;
    bool started() const { return pid > 0; }
};

enum class GameProcessState { Running, Exited, Signaled, Error };
struct GameProcessStatus {
    GameProcessState state = GameProcessState::Error;
    int exit_code = 1;
    std::string message;
};

// Starts a previously checked app without a shell. Each game session writes
// stdout and stderr to a private new log in $XDG_STATE_HOME or ~/.local/state.
// The caller is responsible for polling/reaping the child process.
GameLaunch start_game(const GameProfile& profile);
GameProcessStatus poll_game(pid_t pid);
GameProcessStatus wait_game(pid_t pid);

// CLI convenience wrapper: wait for exit, report the log, return the exit code.
int launch_game(const GameProfile& profile);

} // namespace astra
