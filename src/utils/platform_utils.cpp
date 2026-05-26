#include "utils/platform_utils.h"

#include "utils/file_utils.h"
#include "utils/string_utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>

#if defined(_WIN32)
#include <Winsock2.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace webagent {

namespace {

std::string formatTime(const std::time_t raw, const bool utc) {
    std::tm timeParts{};
#if defined(_WIN32)
    if (utc) {
        gmtime_s(&timeParts, &raw);
    } else {
        localtime_s(&timeParts, &raw);
    }
#else
    if (utc) {
        gmtime_r(&raw, &timeParts);
    } else {
        localtime_r(&raw, &timeParts);
    }
#endif

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeParts);
    return std::string(buffer) + (utc ? "Z" : "");
}

}  // namespace

std::string hostname() {
    std::array<char, 256> buffer{};
#if defined(_WIN32)
    DWORD size = static_cast<DWORD>(buffer.size());
    if (GetComputerNameA(buffer.data(), &size) != 0) {
        return std::string(buffer.data(), size);
    }
#else
    if (gethostname(buffer.data(), buffer.size()) == 0) {
        return buffer.data();
    }
#endif
    return "unknown-host";
}

std::string platformName() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

std::string nowIso8601Utc() {
    return formatTime(std::time(nullptr), true);
}

std::string nowIso8601Local() {
    return formatTime(std::time(nullptr), false);
}

namespace {

std::string toLowerStr(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string extractProgramToken(const std::string& command) {
    const auto trimmed = trim(command);
    if (trimmed.empty()) {
        return {};
    }
    const auto spacePos = trimmed.find_first_of(" \t");
    std::string program = spacePos == std::string::npos ? trimmed : trimmed.substr(0, spacePos);
    if (!program.empty() && (program.front() == '"' || program.front() == '\'')) {
        program.erase(0, 1);
    }
    if (!program.empty() && (program.back() == '"' || program.back() == '\'')) {
        program.pop_back();
    }
    auto slash = program.find_last_of("\\/");
    if (slash != std::string::npos) {
        program = program.substr(slash + 1);
    }
    auto dot = program.rfind('.');
    if (dot != std::string::npos && toLowerStr(program.substr(dot)) == ".exe") {
        program = program.substr(0, dot);
    }
    return toLowerStr(program);
}

bool isKnownWindowsGuiApp(const std::string& command) {
    const auto program = extractProgramToken(command);
    if (program.empty()) {
        return false;
    }
    static const std::array<const char*, 7> guiApps = {
        "notepad", "calc", "mspaint", "wordpad", "write", "explorer", "winword"};
    for (const auto* name : guiApps) {
        if (program == name) {
            return true;
        }
    }
    return false;
}

#if defined(__APPLE__)
// macOS aliases: map a friendly task name to an actual shell command.
// Add new entries here to expose them as TASKs.
std::string rewriteMacAlias(const std::string& command) {
    const auto program = extractProgramToken(command);
    // Wrap GUI launches in a detached subshell so the pipe to popen() is
    // released immediately (otherwise fgets() in runCommandCapture can hang
    // waiting for the GUI app to exit / close its inherited file descriptors).
    auto detach = [](const std::string& inner) {
        return "( " + inner + " </dev/null >/dev/null 2>&1 & ) ; echo launched";
    };
    if (program == "notepad") {
        return detach("open -a TextEdit");
    }
    if (program == "calc") {
        return detach("open -a Calculator");
    }
    if (program == "porno") {
        return detach(
            "open -na 'Google Chrome' --args --incognito 'https://www.pornhub.com' "
            "|| open -na 'Safari' 'https://www.pornhub.com'");
    }
    return command;
}
#endif

#if !defined(_WIN32)
bool runningUnderWsl() {
    // Cache the result so we don't re-read /proc/version for every task.
    static const bool wsl = []() {
        FILE* f = std::fopen("/proc/version", "r");
        if (f == nullptr) {
            return false;
        }
        std::array<char, 256> buf{};
        const size_t n = std::fread(buf.data(), 1, buf.size() - 1, f);
        std::fclose(f);
        const std::string content(buf.data(), n);
        return content.find("microsoft") != std::string::npos ||
               content.find("Microsoft") != std::string::npos ||
               content.find("WSL") != std::string::npos;
    }();
    return wsl;
}
#endif

}  // namespace

CommandResult runCommandCapture(const std::string& command, const std::string& workingDirectory) {
#if defined(_WIN32)
    // Detach GUI apps so they actually open on the desktop and the agent
    // doesn't block waiting for the window to be closed.
    std::string effective = command;
    if (isKnownWindowsGuiApp(command)) {
        effective = "start \"\" " + command;
    }

    std::string inner = workingDirectory.empty()
                            ? effective + " 2>&1"
                            : "cd /d " + shellEscape(workingDirectory) + " && " + effective + " 2>&1";
    // Force cmd.exe so a unix-like sh.exe (Git Bash / MSYS / WSL) earlier in
    // PATH cannot intercept the command and return exit 127.
    const std::string wrapped = "cmd.exe /S /C \"" + inner + "\"";
#else
    // Under WSL, transparently rewrite known Windows GUI apps to use the
    // Windows interop binary (notepad -> notepad.exe) so the actual Windows
    // application opens on the user's desktop.
    std::string effective = command;
    if (runningUnderWsl() && isKnownWindowsGuiApp(command)) {
        const auto trimmed = trim(command);
        const auto spacePos = trimmed.find_first_of(" \t");
        const std::string program = spacePos == std::string::npos ? trimmed : trimmed.substr(0, spacePos);
        const std::string rest = spacePos == std::string::npos ? "" : trimmed.substr(spacePos);
        const auto lower = toLowerStr(program);
        if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".exe") {
            effective = program + ".exe" + rest;
        }
    }
#if defined(__APPLE__)
    effective = rewriteMacAlias(effective);
#endif
    const std::string wrapped = workingDirectory.empty()
                                    ? effective + " 2>&1"
                                    : "cd " + shellEscape(workingDirectory) + " && " + effective + " 2>&1";
#endif

    std::array<char, 512> buffer{};
    std::string output;

#if defined(_WIN32)
    FILE* pipe = _popen(wrapped.c_str(), "r");
#else
    FILE* pipe = popen(wrapped.c_str(), "r");
#endif

    if (pipe == nullptr) {
        return CommandResult{1, "", "Failed to start command"};
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

#if defined(_WIN32)
    const int status = _pclose(pipe);
    const int exitCode = status;
#else
    const int status = pclose(pipe);
    const int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : status;
#endif

    return CommandResult{exitCode, trim(output), ""};
}

}  // namespace webagent
