#include "core/logger.h"

#include "utils/file_utils.h"
#include "utils/platform_utils.h"

#include <fstream>
#include <iostream>

namespace webagent {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::initialize(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    filePath_ = filePath;
    ensureParentDirectory(filePath_);
}

void Logger::info(const std::string& message) {
    log("INFO", message);
}

void Logger::warn(const std::string& message) {
    log("WARN", message);
}

void Logger::error(const std::string& message) {
    log("ERROR", message);
}

void Logger::debug(const std::string& message) {
    log("DEBUG", message);
}

void Logger::log(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string line = nowIso8601Local() + " [" + level + "] " + message + "\n";

    std::cout << line;
    std::cout.flush();

    if (!filePath_.empty()) {
        std::ofstream output(filePath_, std::ios::app);
        output << line;
    }
}

}  // namespace webagent
