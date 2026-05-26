#include "core/agent.h"

#include "core/logger.h"
#include "network/api_types.h"
#include "utils/platform_utils.h"
#include "utils/json.h"

#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace webagent {

Agent::Agent(Config config)
    : config_(std::move(config)), httpClient_(config_), taskExecutor_(config_) {}

int Agent::run() {
    if (!registerAgent()) {
        return 2;
    }

    std::vector<std::future<void>> runningTasks;
    std::mutex taskExecutorMutex;
    auto runWithRetry = [&](const std::function<std::string()>& action, const std::string& op) {
        for (int attempt = 1; attempt <= config_.retryAttempts; ++attempt) {
            try {
                return action();
            } catch (const std::exception& ex) {
                Logger::instance().warn(op + " failed (attempt " + std::to_string(attempt) + "/" +
                                        std::to_string(config_.retryAttempts) + "): " + ex.what());
                if (attempt == config_.retryAttempts) {
                    throw;
                }
                std::this_thread::sleep_for(std::chrono::seconds(config_.retryBackoffSec));
            }
        }
        return std::string();
    };

    const bool useLegacyApi = config_.serverUrl.find("/app/webagent1/api") != std::string::npos;
    const std::string taskPath = useLegacyApi ? "/wa_task/" : "/api/v1/agent/task";
    const std::string resultPath = useLegacyApi ? "/wa_result/" : "/api/v1/agent/result";

    int cycle = 0;
    while (!shouldStop(cycle)) {
        ++cycle;

        // Drain any finished background tasks so they don't pile up and so we
        // never have to block on .get() of an already-completed future from a
        // previous cycle (which was the cause of the loop appearing to "hang"
        // after the first task).
        for (auto it = runningTasks.begin(); it != runningTasks.end();) {
            if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                try {
                    it->get();
                } catch (const std::exception& ex) {
                    Logger::instance().error("background task failed: " +
                                             std::string(ex.what()));
                }
                it = runningTasks.erase(it);
            } else {
                ++it;
            }
        }

        Logger::instance().debug("poll cycle #" + std::to_string(cycle) +
                                 " (pending=" + std::to_string(runningTasks.size()) + ")");

        try {
            const auto responseBody = runWithRetry(
                [&]() {
                    return httpClient_.postJson(makeUrl(taskPath),
                                                toTaskPollJson(config_.uid, config_.descr,
                                                               config_.accessCode)
                                                    .stringify(),
                                                "task");
                },
                "wa_task");

            const auto response = parseTaskPollResponse(Json::parse(responseBody));
            if (!response.tasks.empty()) {
                auto processTask = [&](TaskDefinition task) {
                    task.uid = config_.uid;
                    task.accessCode = config_.accessCode;
                    Logger::instance().info("Received task_code=" + task.taskCode +
                                            " session_id=" + task.sessionId);
                    TaskResult result;
                    {
                        std::lock_guard<std::mutex> lock(taskExecutorMutex);
                        result = taskExecutor_.execute(task);
                    }
                    const std::function<std::string()> sendResult = [&]() {
                        return httpClient_.postMultipartResult(makeUrl(resultPath),
                                                               result.resultCode,
                                                               toResultJsonString(result),
                                                               result.files);
                    };
                    const auto uploadResponse = runWithRetry(sendResult, "wa_result");
                    Logger::instance().info("wa_result response: " + uploadResponse);
                };

                for (TaskDefinition task : response.tasks) {
                    if (config_.maxParallelTasks <= 1) {
                        // Run inline — simpler, no async/future plumbing.
                        try {
                            processTask(task);
                        } catch (const std::exception& ex) {
                            Logger::instance().error(
                                "task processing failed: " + std::string(ex.what()));
                        }
                    } else {
                        while (static_cast<int>(runningTasks.size()) >=
                               config_.maxParallelTasks) {
                            runningTasks.front().get();
                            runningTasks.erase(runningTasks.begin());
                        }
                        runningTasks.push_back(std::async(std::launch::async,
                                                          [processTask, task]() mutable {
                                                              processTask(std::move(task));
                                                          }));
                    }
                }
            } else if (response.codeResponse == "0") {
                Logger::instance().debug("wa_task returned WAIT");
            } else {
                Logger::instance().warn("wa_task response without tasks: " + responseBody);
            }
        } catch (const std::exception& ex) {
            Logger::instance().error("poll cycle failed: " + std::string(ex.what()));
            std::this_thread::sleep_for(std::chrono::seconds(config_.retryBackoffSec));
        }

        std::this_thread::sleep_for(std::chrono::seconds(config_.pollIntervalSec));
    }

    for (auto& pending : runningTasks) {
        pending.get();
    }

    return 0;
}

bool Agent::registerAgent() {
    if (!config_.accessCode.empty()) {
        Logger::instance().info("Using saved access_code for UID=" + config_.uid);
        return true;
    }

    const bool useLegacyApi = config_.serverUrl.find("/app/webagent1/api") != std::string::npos;
    const std::string registerPath = useLegacyApi ? "/wa_reg/" : "/api/v1/agent/register";

    const auto body = httpClient_.postJson(makeUrl(registerPath),
                                           toRegistrationJson(config_.uid, config_.descr, hostname(),
                                                              platformName(), config_.agentVersion)
                                               .stringify(),
                                           "register");
    const auto response = parseRegistrationResponse(Json::parse(body));

    if (response.codeResponse == "0" && !response.accessCode.empty()) {
        config_.accessCode = response.accessCode;
        config_.saveState();
        Logger::instance().info("Registration success; access_code stored in " + config_.stateFile);
        return true;
    }

    if (response.codeResponse == "-3") {
        Logger::instance().error(
            "Agent already registered, but access_code was not present locally. "
            "Set access_code in config or use a new UID.");
        return false;
    }

    Logger::instance().error("Registration failed: " + body);
    return false;
}

std::string Agent::makeUrl(const std::string& path) const {
    if (config_.serverUrl.empty()) {
        return path;
    }
    if (config_.serverUrl.back() == '/' && !path.empty() && path.front() == '/') {
        return config_.serverUrl.substr(0, config_.serverUrl.size() - 1) + path;
    }
    if (config_.serverUrl.back() != '/' && !path.empty() && path.front() != '/') {
        return config_.serverUrl + "/" + path;
    }
    return config_.serverUrl + path;
}

bool Agent::shouldStop(const int cycle) const noexcept {
    return config_.maxCycles > 0 && cycle >= config_.maxCycles;
}

}  // namespace webagent
