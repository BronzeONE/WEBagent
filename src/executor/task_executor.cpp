#include "executor/task_executor.h"

#include "utils/file_utils.h"
#include "utils/platform_utils.h"
#include "utils/string_utils.h"

#include <stdexcept>

namespace webagent {

namespace {

std::string makeSessionFileName(const std::string& prefix, const std::string& sessionId, const std::string& ext) {
    std::string sanitized = sessionId;
    for (char& ch : sanitized) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '_')) {
            ch = '_';
        }
    }
    return prefix + "_" + sanitized + ext;
}

TaskResult makeBaseResult(const Config& config, const TaskDefinition& task) {
    TaskResult result;
    result.uid = config.uid;
    result.accessCode = task.accessCode;
    result.sessionId = task.sessionId;
    return result;
}

std::string extractProgramName(const std::string& command) {
    const auto trimmed = trim(command);
    const auto pos = trimmed.find(' ');
    if (pos == std::string::npos) {
        return trimmed;
    }
    return trimmed.substr(0, pos);
}

}  // namespace

TaskExecutor::TaskExecutor(Config& config) : config_(config) {}

TaskResult TaskExecutor::execute(const TaskDefinition& task) {
    switch (task.type) {
        case TaskType::Config:
            return executeConfigTask(task);
        case TaskType::File:
            return executeFileTask(task);
        case TaskType::Task:
        case TaskType::RunCommand:
        case TaskType::RunProgram:
        case TaskType::GenerateDocument:
        case TaskType::CollectFiles:
        case TaskType::UploadFile:
        case TaskType::PrintDocument:
            return executeProgramTask(task);
        case TaskType::Timeout:
            return executeTimeoutTask(task);
        default: {
            TaskResult result = makeBaseResult(config_, task);
            result.resultCode = -1;
            result.message = "Unsupported task_code: " + task.taskCode;
            return result;
        }
    }
}

TaskResult TaskExecutor::executeConfigTask(const TaskDefinition& task) const {
    const std::string optionsFile = joinPath(config_.taskDirectory, "last_task_options.txt");
    writeTextFile(optionsFile, task.options);
    config_.markServerUpdate(task.taskCode, task.options,
                             {"last_server_task_code", "last_server_options"});
    config_.saveConfig();

    TaskResult result = makeBaseResult(config_, task);
    result.resultCode = 0;
    result.message = task.options.empty() ? "CONF task processed" : "CONF options saved";
    result.files.push_back(optionsFile);
    return result;
}

TaskResult TaskExecutor::executeFileTask(const TaskDefinition& task) const {
    TaskResult result = makeBaseResult(config_, task);
    config_.markServerUpdate(task.taskCode, task.options,
                             {"last_server_task_code", "last_server_options"});
    config_.saveConfig();

    const std::string rawOptions = trim(task.options);
    std::string filePath;

    if (!rawOptions.empty() && fileExists(rawOptions)) {
        filePath = rawOptions;
    } else if (!rawOptions.empty()) {
        const std::string candidate = joinPath(config_.resultDirectory, rawOptions);
        if (fileExists(candidate)) {
            filePath = candidate;
        }
    }

    if (filePath.empty()) {
        filePath = joinPath(
            config_.resultDirectory, makeSessionFileName("file_task_result", task.sessionId, ".txt"));
        std::string content = "FILE task processed\n";
        content += "session_id: " + task.sessionId + "\n";
        content += "uid: " + config_.uid + "\n";
        content += "options: " + (rawOptions.empty() ? "<empty>" : rawOptions) + "\n";
        writeTextFile(filePath, content);
        result.message = rawOptions.empty() ? "FILE task processed" : "FILE task fallback file created";
    } else {
        result.message = "FILE task processed";
    }

    result.resultCode = 0;
    result.files.push_back(filePath);
    return result;
}

TaskResult TaskExecutor::executeProgramTask(const TaskDefinition& task) const {
    TaskResult result = makeBaseResult(config_, task);
    config_.markServerUpdate(task.taskCode, task.options,
                             {"last_server_task_code", "last_server_options"});
    config_.saveConfig();

    const std::string command = trim(task.options).empty() ? "echo TASK executed" : task.options;
    const std::string programName = extractProgramName(command);
    if (!programName.empty() && !config_.isProgramAllowed(programName)) {
        result.resultCode = -1;
        result.message = "Program is not allowed: " + programName;
        return result;
    }
    const auto commandResult = runCommandCapture(command);

    const std::string outputFile = joinPath(
        config_.resultDirectory, makeSessionFileName("task_output", task.sessionId, ".txt"));

    std::string content = "TASK task processed\n";
    content += "command: " + command + "\n";
    content += "exit_code: " + std::to_string(commandResult.exitCode) + "\n";
    content += "output:\n" + commandResult.stdoutOutput + "\n";
    writeTextFile(outputFile, content);

    result.files.push_back(outputFile);

    if (commandResult.exitCode == 0) {
        result.resultCode = 0;
        result.message = commandResult.stdoutOutput.empty() ? "TASK task processed"
                                                            : commandResult.stdoutOutput;
    } else {
        result.resultCode = -1;
        result.message = "TASK command failed with exit code " +
                         std::to_string(commandResult.exitCode);
    }

    return result;
}

TaskResult TaskExecutor::executeTimeoutTask(const TaskDefinition& task) {
    TaskResult result = makeBaseResult(config_, task);

    const std::string rawOptions = trim(task.options);
    int timeoutValue = config_.pollIntervalSec;

    if (!rawOptions.empty()) {
        try {
            timeoutValue = std::stoi(rawOptions);
        } catch (const std::exception&) {
            result.resultCode = -1;
            result.message = "TIMEOUT task invalid value: " + rawOptions;
            return result;
        }
    }

    if (timeoutValue < 1) {
        result.resultCode = -1;
        result.message = "TIMEOUT must be >= 1";
        return result;
    }
    // Cap the poll interval so a runaway server value can't put the agent to
    // sleep for hours/days. 1 hour is plenty for any reasonable use case.
    constexpr int kMaxPollIntervalSec = 3600;
    if (timeoutValue > kMaxPollIntervalSec) {
        timeoutValue = kMaxPollIntervalSec;
    }

    config_.pollIntervalSec = timeoutValue;
    config_.markServerUpdate(task.taskCode, task.options,
                             {"last_server_task_code", "last_server_options",
                              "poll_interval_sec"});
    config_.saveConfig();

    const std::string outputFile = joinPath(
        config_.taskDirectory, makeSessionFileName("timeout_update", task.sessionId, ".txt"));
    writeTextFile(outputFile,
                  "TIMEOUT task processed\nnew_poll_interval_sec=" +
                      std::to_string(config_.pollIntervalSec) + "\n");

    result.resultCode = 0;
    result.message = "TIMEOUT updated to " + std::to_string(config_.pollIntervalSec) + " sec";
    result.files.push_back(outputFile);
    return result;
}

}  // namespace webagent
