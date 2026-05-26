#include "network/api_types.h"

namespace webagent {

TaskType taskTypeFromString(const std::string& value) {
    if (value == "run_command") {
        return TaskType::RunCommand;
    }
    if (value == "run_program") {
        return TaskType::RunProgram;
    }
    if (value == "generate_document") {
        return TaskType::GenerateDocument;
    }
    if (value == "collect_files") {
        return TaskType::CollectFiles;
    }
    if (value == "upload_file") {
        return TaskType::UploadFile;
    }
    if (value == "print_document") {
        return TaskType::PrintDocument;
    }
    if (value == "CONF") {
        return TaskType::Config;
    }
    if (value == "FILE") {
        return TaskType::File;
    }
    if (value == "TASK") {
        return TaskType::Task;
    }
    if (value == "TIMEOUT") {
        return TaskType::Timeout;
    }
    return TaskType::Unknown;
}

Json toRegistrationJson(const std::string& uid,
                        const std::string& descr,
                        const std::string& hostName,
                        const std::string& platform,
                        const std::string& version) {
    return Json::object{
        {"UID", uid},
        {"uid", uid},
        {"descr", descr},
        {"hostname", hostName},
        {"platform", platform},
        {"version", version},
    };
}

Json toTaskPollJson(const std::string& uid, const std::string& descr, const std::string& accessCode) {
    return Json::object{
        {"UID", uid},
        {"descr", descr},
        {"access_code", accessCode},
    };
}

std::string toResultJsonString(const TaskResult& value) {
    return Json(Json::object{
                    {"UID", value.uid},
                    {"access_code", value.accessCode},
                    {"message", value.message},
                    {"files", static_cast<double>(value.files.size())},
                    {"session_id", value.sessionId},
                })
        .stringify();
}

RegistrationResponse parseRegistrationResponse(const Json& json) {
    RegistrationResponse response;
    response.codeResponse = json.getString("code_responce", "");
    response.message = json.getString("msg", "");
    response.accessCode = json.getString("access_code", "");
    return response;
}

TaskPollResponse parseTaskPollResponse(const Json& json) {
    TaskPollResponse response;
    response.codeResponse = json.getString("code_responce", "");
    response.status = json.getString("status", "");
    response.message = json.getString("msg", "");

    auto parseTaskObject = [&](const Json& source) {
        TaskDefinition task;
        task.id = source.getString("id", source.getString("task_id", ""));
        task.taskCode = source.getString("task_code", source.getString("type", ""));
        task.type = taskTypeFromString(task.taskCode);
        task.options = source.getString("options", source.getString("command", ""));
        task.sessionId = source.getString("session_id", "");
        task.status = response.status;
        return task;
    };

    if (const auto* tasks = json.find("tasks"); tasks != nullptr && tasks->isArray()) {
        for (const auto& entry : tasks->arrayItems()) {
            if (entry.isObject()) {
                response.tasks.push_back(parseTaskObject(entry));
            }
        }
    } else if (response.codeResponse == "1") {
        response.task = parseTaskObject(json);
        response.tasks.push_back(response.task);
    }

    return response;
}

}  // namespace webagent
