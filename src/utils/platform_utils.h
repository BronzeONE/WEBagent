#pragma once

#include <string>

namespace webagent {

struct CommandResult {
    int exitCode = 0;
    std::string stdoutOutput;
    std::string stderrOutput;
};

std::string hostname();
std::string platformName();
std::string nowIso8601Utc();
std::string nowIso8601Local();
CommandResult runCommandCapture(const std::string& command, const std::string& workingDirectory = "");

}  // namespace webagent
