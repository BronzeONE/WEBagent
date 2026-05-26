#pragma once

#include <mutex>
#include <string>

namespace webagent {

class Logger {
public:
    static Logger& instance();

    void initialize(const std::string& filePath);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    void debug(const std::string& message);

private:
    Logger() = default;
    void log(const std::string& level, const std::string& message);

    std::mutex mutex_;
    std::string filePath_;
};

}  // namespace webagent
