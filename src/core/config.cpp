#include "core/config.h"

#include "utils/file_utils.h"
#include "utils/json.h"
#include "utils/string_utils.h"
#include "utils/platform_utils.h"

#include <stdexcept>

namespace webagent {

namespace {

Json::object buildConfigJsonObject(const Config& config, Json::object root = {}) {
    root["uid"] = config.uid;
    root["descr"] = config.descr;
    root["server_url"] = config.serverUrl;
    root["api_token"] = config.apiToken;
    root["agent_version"] = config.agentVersion;
    root["access_code"] = config.accessCode;
    root["poll_interval_sec"] = config.pollIntervalSec;
    root["poll_interval"] = config.pollIntervalSec;
    root["max_parallel_tasks"] = config.maxParallelTasks;
    root["task_directory"] = config.taskDirectory;
    root["result_directory"] = config.resultDirectory;
    root["log_file"] = config.logFile;
    root["state_file"] = config.stateFile;
    root["connect_timeout_sec"] = config.connectTimeoutSec;
    root["request_timeout_sec"] = config.requestTimeoutSec;
    root["retry_backoff_sec"] = config.retryBackoffSec;
    root["retry_attempts"] = config.retryAttempts;
    root["verify_tls"] = config.verifyTls;
    root["ca_cert_path"] = config.caCertPath;
    root["mock_api_directory"] = config.mockApiDirectory;
    root["max_cycles"] = config.maxCycles;
    root["last_server_task_code"] = config.lastServerTaskCode;
    root["last_server_options"] = config.lastServerOptions;
    root["last_updated_at"] = config.lastUpdatedAt;

    Json::array allowedProgramsJson;
    for (const auto& program : config.allowedPrograms) {
        allowedProgramsJson.push_back(Json(program));
    }
    root["allowed_programs"] = Json(allowedProgramsJson);

    Json::array changedFieldsJson;
    for (const auto& field : config.lastChangedFields) {
        changedFieldsJson.push_back(Json(field));
    }
    root["last_changed_fields"] = Json(changedFieldsJson);
    root["last_server_update"] = Json(Json::object{
        {"task_code", config.lastServerTaskCode},
        {"options", config.lastServerOptions},
        {"updated_at", config.lastUpdatedAt},
        {"changed_fields", Json(changedFieldsJson)},
    });

    return root;
}

}  // namespace

bool Config::isMockMode() const noexcept {
    return !mockApiDirectory.empty();
}

bool Config::isProgramAllowed(const std::string& program) const {
    if (allowedPrograms.empty()) {
        return true;
    }

    for (const auto& allowed : allowedPrograms) {
        if (allowed == program) {
            return true;
        }
    }

    return false;
}

void Config::saveState() const {
    writeTextFile(stateFile,
                  Json(Json::object{
                           {"uid", uid},
                           {"descr", descr},
                           {"server_url", serverUrl},
                           {"api_token", apiToken},
                           {"access_code", accessCode},
                       })
                      .stringify());
}

void Config::saveConfig() const {
    Json::object root;

    if (!configPath.empty() && fileExists(configPath)) {
        const auto current = Json::parse(readTextFile(configPath));
        if (current.isObject()) {
            root = current.objectItems();
        }
    }

    if (configPath.empty()) {
        throw std::runtime_error("Config path is empty; cannot save config");
    }

    writeTextFile(configPath, Json(buildConfigJsonObject(*this, root)).stringify());

    const std::string mainConfigPath = "config.json";
    if (configPath != mainConfigPath) {
        Json::object mainRoot;
        if (fileExists(mainConfigPath)) {
            const auto currentMain = Json::parse(readTextFile(mainConfigPath));
            if (currentMain.isObject()) {
                mainRoot = currentMain.objectItems();
            }
        }
        writeTextFile(mainConfigPath, Json(buildConfigJsonObject(*this, mainRoot)).stringify());
    }
}

void Config::markServerUpdate(const std::string& taskCode,
                              const std::string& options,
                              const std::vector<std::string>& changedFields) {
    lastServerTaskCode = taskCode;
    lastServerOptions = options;
    lastUpdatedAt = nowIso8601Local();
    lastChangedFields = changedFields;
}

Config Config::loadFromFile(const std::string& path) {
    const auto content = readTextFile(path);
    const auto root = Json::parse(content);
    if (!root.isObject()) {
        throw std::runtime_error("Config must be a JSON object");
    }

    Config config;
    config.configPath = path;
    config.uid = root.getString("uid");
    config.descr = root.getString("descr", "web-agent");
    config.serverUrl = root.getString("server_url");
    config.apiToken = root.getString("api_token", "");
    config.agentVersion = root.getString("agent_version", "1.0.0");
    config.accessCode = root.getString("access_code", "");
    config.pollIntervalSec = static_cast<int>(root.getNumber(
        "poll_interval_sec", root.getNumber("poll_interval", 10)));
    config.maxParallelTasks = static_cast<int>(root.getNumber("max_parallel_tasks", 1));
    config.taskDirectory = root.getString("task_directory", "./tasks");
    config.resultDirectory = root.getString("result_directory", "./results");
    config.logFile = root.getString("log_file", "./agent.log");
    config.stateFile = root.getString("state_file", "./agent_state.json");
    config.connectTimeoutSec = static_cast<int>(root.getNumber("connect_timeout_sec", 5));
    config.requestTimeoutSec = static_cast<int>(root.getNumber("request_timeout_sec", 30));
    config.retryBackoffSec = static_cast<int>(root.getNumber("retry_backoff_sec", 60));
    config.retryAttempts = static_cast<int>(root.getNumber("retry_attempts", 3));
    config.verifyTls = root.getBool("verify_tls", true);
    config.caCertPath = root.getString("ca_cert_path", "");
    config.mockApiDirectory = root.getString("mock_api_directory", "");
    config.maxCycles = static_cast<int>(root.getNumber("max_cycles", 0));
    config.lastServerTaskCode = root.getString("last_server_task_code", "");
    config.lastServerOptions = root.getString("last_server_options", "");
    config.lastUpdatedAt = root.getString("last_updated_at", "");

    if (const auto* changed = root.find("last_changed_fields"); changed != nullptr && changed->isArray()) {
        for (const auto& entry : changed->arrayItems()) {
            if (entry.isString()) {
                config.lastChangedFields.push_back(entry.asString());
            }
        }
    }

    if (const auto* allowed = root.find("allowed_programs"); allowed != nullptr && allowed->isArray()) {
        for (const auto& entry : allowed->arrayItems()) {
            if (entry.isString()) {
                config.allowedPrograms.push_back(entry.asString());
            }
        }
    }

    if (trim(config.uid).empty()) {
        throw std::runtime_error("Config field 'uid' is required");
    }
    if (trim(config.serverUrl).empty() && !config.isMockMode()) {
        throw std::runtime_error("Config field 'server_url' is required unless mock_api_directory is set");
    }
    if (config.pollIntervalSec < 1) {
        throw std::runtime_error("poll_interval_sec must be positive");
    }
    if (config.maxParallelTasks < 1) {
        throw std::runtime_error("max_parallel_tasks must be positive");
    }
    if (config.retryAttempts < 1) {
        throw std::runtime_error("retry_attempts must be positive");
    }

    ensureDirectory(config.taskDirectory);
    ensureDirectory(config.resultDirectory);

    if (config.accessCode.empty() && fileExists(config.stateFile)) {
        const auto saved = Json::parse(readTextFile(config.stateFile));
        config.accessCode = saved.getString("access_code", "");
    }

    return config;
}

}  // namespace webagent
