#pragma once

#include <string>
#include <vector>

namespace webagent {

struct Config {
    std::string configPath;
    std::string uid;
    std::string descr = "web-agent";
    std::string serverUrl;
    std::string apiToken;
    std::string agentVersion = "1.0.0";
    std::string accessCode;
    int pollIntervalSec = 10;
    int maxParallelTasks = 1;
    std::string taskDirectory = "./tasks";
    std::string resultDirectory = "./results";
    std::vector<std::string> allowedPrograms;
    std::string logFile = "./agent.log";
    std::string stateFile = "./agent_state.json";
    int connectTimeoutSec = 5;
    int requestTimeoutSec = 30;
    int retryBackoffSec = 60;
    int retryAttempts = 3;
    bool verifyTls = true;
    std::string caCertPath;
    std::string mockApiDirectory;
    int maxCycles = 0;
    std::string lastServerTaskCode;
    std::string lastServerOptions;
    std::string lastUpdatedAt;
    std::vector<std::string> lastChangedFields;

    [[nodiscard]] bool isMockMode() const noexcept;
    [[nodiscard]] bool isProgramAllowed(const std::string& program) const;
    void saveState() const;
    void saveConfig() const;
    void markServerUpdate(const std::string& taskCode, const std::string& options,
                          const std::vector<std::string>& changedFields);

    static Config loadFromFile(const std::string& path);
};

}  // namespace webagent
