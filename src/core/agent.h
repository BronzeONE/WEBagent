#pragma once

#include "core/config.h"
#include "executor/task_executor.h"
#include "network/http_client.h"

#include <string>

namespace webagent {

class Agent {
public:
    explicit Agent(Config config);
    int run();

private:
    bool registerAgent();
    std::string makeUrl(const std::string& path) const;
    bool shouldStop(int cycle) const noexcept;

    Config config_;
    HttpClient httpClient_;
    TaskExecutor taskExecutor_;
};

}  // namespace webagent
