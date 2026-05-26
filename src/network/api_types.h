#pragma once

#include "utils/json.h"

#include <string>
#include <vector>

namespace webagent {

enum class TaskType {
    RunCommand,
    RunProgram,
    GenerateDocument,
    CollectFiles,
    UploadFile,
    PrintDocument,
    Config,
    File,
    Task,
    Timeout,
    Unknown
};

struct RegistrationResponse {
    std::string codeResponse;
    std::string message;
    std::string accessCode;
};

struct TaskDefinition {
    std::string id;
    std::string uid;
    std::string accessCode;
    std::string sessionId;
    std::string status;
    std::string taskCode;
    TaskType type = TaskType::Unknown;
    std::string options;
};

struct TaskPollResponse {
    std::string codeResponse;
    std::string status;
    std::string message;
    TaskDefinition task;
    std::vector<TaskDefinition> tasks;
};

struct TaskResult {
    int resultCode = 0;
    std::string uid;
    std::string accessCode;
    std::string sessionId;
    std::string message;
    std::vector<std::string> files;
};

TaskType taskTypeFromString(const std::string& value);
Json toRegistrationJson(const std::string& uid,
                        const std::string& descr,
                        const std::string& hostName,
                        const std::string& platform,
                        const std::string& version);
Json toTaskPollJson(const std::string& uid, const std::string& descr, const std::string& accessCode);
std::string toResultJsonString(const TaskResult& value);
RegistrationResponse parseRegistrationResponse(const Json& json);
TaskPollResponse parseTaskPollResponse(const Json& json);

}  // namespace webagent
