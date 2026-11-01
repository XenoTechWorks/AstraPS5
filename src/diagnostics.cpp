// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
#include "astraps5/diagnostics.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <thread>

namespace fs = std::filesystem;
namespace astra {
namespace {

std::string trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string read_file(const fs::path& path) {
    std::ifstream file(path);
    if (!file) return {};
    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (contents.size() > 32768) contents.resize(32768);
    return trim(contents);
}

bool path_exists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

bool is_executable(const fs::path& path) {
    return access(path.c_str(), X_OK) == 0;
}

std::string binary_path(std::string_view name) {
    const char* env = std::getenv("PATH");
    if (!env) return {};
    std::string paths(env);
    std::size_t from = 0;
    while (from <= paths.size()) {
        auto to = paths.find(':', from);
        if (to == std::string::npos) to = paths.size();
        fs::path base = paths.substr(from, to - from);
        if (base.empty()) base = ".";
        const auto candidate = base / std::string(name);
        if (is_executable(candidate)) return candidate.string();
        if (to == paths.size()) break;
        from = to + 1;
    }
    return {};
}

std::string first_render_node() {
    std::error_code ec;
    if (!fs::is_directory("/dev/dri", ec)) return {};
    for (const auto& item : fs::directory_iterator("/dev/dri", ec)) {
        auto name = item.path().filename().string();
        if (name.rfind("renderD", 0) == 0) return item.path().string();
    }
    return {};
}

bool has_active_network_link() {
    std::error_code ec;
    const fs::path root = "/sys/class/net";
    if (!fs::is_directory(root, ec)) return false;
    for (const auto& item : fs::directory_iterator(root, ec)) {
        if (item.path().filename() == "lo") continue;
        if (read_file(item.path() / "operstate") == "up") return true;
    }
    return false;
}

bool has_controller() {
    std::error_code ec;
    const fs::path root = "/sys/class/input";
    if (!fs::is_directory(root, ec)) return false;
    for (const auto& item : fs::directory_iterator(root, ec)) {
        const auto name = read_file(item.path() / "device/name");
        if (name.find("DualSense") != std::string::npos ||
            name.find("Wireless Controller") != std::string::npos) return true;
    }
    return false;
}

bool steam_present() {
    if (!binary_path("steam").empty()) return true;
    const char* home = std::getenv("HOME");
    if (!home) return false;
    const fs::path h(home);
    return path_exists(h / ".steam/steam/steam.sh") ||
           path_exists(h / ".local/share/Steam/steam.sh");
}

bool proton_present() {
    const char* home = std::getenv("HOME");
    if (!home) return false;
    const fs::path h(home);
    const fs::path dirs[] = {h / ".steam/root/steamapps/common", h / ".local/share/Steam/steamapps/common",
                             h / ".steam/steam/compatibilitytools.d", h / ".local/share/Steam/compatibilitytools.d"};
    std::error_code ec;
    for (const auto& directory : dirs) {
        if (!fs::is_directory(directory, ec)) continue;
        for (const auto& entry : fs::directory_iterator(directory, ec)) {
            std::string name = entry.path().filename().string();
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {return char(std::tolower(c));});
            if (name.find("proton") != std::string::npos || name.find("ge-proton") != std::string::npos) return true;
        }
    }
    return false;
}

struct CommandResult { bool success = false; bool timed_out = false; std::string output; };

// Executes ONLY a fixed executable name and fixed argument: no shell, no root, no user input.
CommandResult run_vulkaninfo() {
    CommandResult result;
    int fd[2];
    if (pipe(fd) != 0) return result;
    const pid_t child = fork();
    if (child == -1) { close(fd[0]); close(fd[1]); return result; }
    if (child == 0) {
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        dup2(fd[1], STDERR_FILENO);
        close(fd[1]);
        char* const args[] = {const_cast<char*>("vulkaninfo"), const_cast<char*>("--summary"), nullptr};
        execvp(args[0], args);
        _exit(127);
    }
    close(fd[1]);
    const int flags = fcntl(fd[0], F_GETFL, 0);
    if (flags != -1) fcntl(fd[0], F_SETFL, flags | O_NONBLOCK);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    int code = 0;
    bool done = false;
    while (std::chrono::steady_clock::now() < deadline) {
        char buffer[2048];
        ssize_t bytes;
        while ((bytes = read(fd[0], buffer, sizeof(buffer))) > 0) {
            if (result.output.size() < 32768) {
                const auto amount = std::min(static_cast<std::size_t>(bytes), 32768 - result.output.size());
                result.output.append(buffer, amount);
            }
        }
        if (waitpid(child, &code, WNOHANG) == child) { done = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!done) {
        result.timed_out = true;
        kill(child, SIGKILL);
        waitpid(child, &code, 0);
    }
    char buffer[2048];
    ssize_t bytes;
    while ((bytes = read(fd[0], buffer, sizeof(buffer))) > 0 && result.output.size() < 32768) {
        result.output.append(buffer, std::min(static_cast<std::size_t>(bytes), 32768 - result.output.size()));
    }
    close(fd[0]);
    result.success = done && WIFEXITED(code) && WEXITSTATUS(code) == 0;
    return result;
}

} // namespace

std::string escape_json(std::string_view text) {
    std::ostringstream out;
    for (unsigned char c : text) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) out << "\\u" << std::hex << std::setfill('0') << std::setw(4) << int(c) << std::dec;
                else out << static_cast<char>(c);
        }
    }
    return out.str();
}

bool contains_ps5_marker(std::string_view text) {
    std::string s(text);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return char(std::tolower(c)); });
    return s.find("playstation 5") != std::string::npos ||
           s.find("prospero") != std::string::npos ||
           s.find("ps5-linux") != std::string::npos ||
           s.find("ps5_") != std::string::npos;
}

std::string extract_vulkan_device(std::string_view output) {
    std::istringstream input{std::string(output)};
    std::string line;
    while (std::getline(input, line)) {
        auto pos = line.find("deviceName");
        if (pos == std::string::npos) continue;
        pos = line.find('=', pos);
        if (pos == std::string::npos) continue;
        auto value = trim(line.substr(pos + 1));
        if (!value.empty()) return value;
    }
    return {};
}

Report collect_diagnostics() {
    Report report;
    auto add = [&report](std::string id, std::string status, std::string detail) {
        report.checks.push_back({std::move(id), std::move(status), std::move(detail)});
    };

    struct utsname system{};
    if (uname(&system) != 0) {
        add("linux", "warning", "Unable to read system information.");
    } else {
        add("linux", std::strcmp(system.sysname, "Linux") == 0 ? "ok" : "warning", system.sysname);
        add("architecture", std::strcmp(system.machine, "x86_64") == 0 ? "ok" : "warning", system.machine);
        add("kernel", "info", system.release);
    }

    const auto product = read_file("/sys/class/dmi/id/product_name");
    const auto version = read_file("/proc/version");
    if (contains_ps5_marker(product) || contains_ps5_marker(version) ||
        contains_ps5_marker(read_file("/proc/cmdline"))) {
        add("ps5_detection", "info", "PS5-related marker detected; hardware identity is NOT verified.");
    } else {
        add("ps5_detection", "info", "No PS5 marker detected. Normal for development on a PC.");
    }

    auto render = first_render_node();
    add("render_node", render.empty() ? "warning" : "ok",
        render.empty() ? "No /dev/dri/renderD* node found." : "Detected " + render + " (not a GPU functionality test).");

    const auto vulkan = binary_path("vulkaninfo");
    if (vulkan.empty()) {
        add("vulkan", "warning", "vulkaninfo not found. Install vulkan-tools to test Vulkan devices.");
    } else {
        const auto result = run_vulkaninfo();
        const auto device = extract_vulkan_device(result.output);
        if (result.success && !device.empty()) {
            add("vulkan", "ok", "vulkaninfo reports device: " + device + ". Rendering not yet tested.");
        } else if (result.timed_out) {
            add("vulkan", "warning", "vulkaninfo timed out after 5 seconds.");
        } else {
            add("vulkan", "warning", "vulkaninfo did not report a usable device. Check Mesa/ICD/drivers.");
        }
    }

    add("steam", steam_present() ? "ok" : "info",
        steam_present() ? "Steam launcher found; launch ability not tested." : "Steam launcher not detected.");
    add("wine", binary_path("wine").empty() ? "info" : "ok",
        binary_path("wine").empty() ? "Wine not found on PATH (optional)." : "Wine executable found; game compatibility not tested.");
    add("proton", proton_present() ? "ok" : "info",
        proton_present() ? "Proton installation directory detected; games not tested." : "Proton not found in standard Steam paths (may still be installed elsewhere).");
    add("controller", has_controller() ? "ok" : "info",
        has_controller() ? "DualSense-like controller detected; input not tested." : "No DualSense-like controller currently detected.");
    add("network", has_active_network_link() ? "ok" : "info",
        has_active_network_link() ? "An interface has link state UP; internet access is NOT verified." : "No non-loopback interface reports link state UP.");
    return report;
}

std::string report_json(const Report& report) {
    std::ostringstream out;
    out << "{\n  \"project\": \"AstraPS5\",\n  \"version\": \"0.1.0-dev\",\n"
        << "  \"phase\": \"experimental-test-project\",\n  \"checks\": [\n";
    for (std::size_t i = 0; i < report.checks.size(); ++i) {
        const auto& check = report.checks[i];
        out << "    {\"id\": \"" << escape_json(check.id) << "\", \"status\": \""
            << escape_json(check.status) << "\", \"detail\": \"" << escape_json(check.detail) << "\"}";
        if (i + 1 != report.checks.size()) out << ',';
        out << '\n';
    }
    out << "  ]\n}\n";
    return out.str();
}

std::string report_text(const Report& report) {
    std::ostringstream out;
    out << "AstraPS5 v0.1.0-dev | Experimental Test Project\n"
        << "Read-only host diagnostics; does not install or change anything.\n\n";
    for (const auto& check : report.checks) {
        out << '[' << (check.status == "ok" ? "OK" : (check.status == "warning" ? "WARN" : "INFO"))
            << "] " << check.id << ": " << check.detail << '\n';
    }
    out << "\nAn OK result is a detected prerequisite, NOT proof any PS5 or PC game is playable.\n";
    return out.str();
}

} // namespace astra
