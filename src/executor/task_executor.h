#pragma once

#include "core/config.h"
#include "network/api_types.h"

namespace webagent {

class TaskExecutor {
public:
    explicit TaskExecutor(Config& config);
    TaskResult execute(const TaskDefinition& task);

private:
    TaskResult executeConfigTask(const TaskDefinition& task) const;
    TaskResult executeFileTask(const TaskDefinition& task) const;
    TaskResult executeProgramTask(const TaskDefinition& task) const;
    TaskResult executeTimeoutTask(const TaskDefinition& task);
    TaskResult executeAliasTask(const TaskDefinition& task) const;

    Config& config_;
};

}  // namespace webagent
