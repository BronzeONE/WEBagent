#include "core/agent.h"
#include "utils/file_utils.h"

#include "test_helpers.h"

#include <gtest/gtest.h>

namespace webagent {

namespace {
Config makeMockConfig(const std::string& dir) {
    Config cfg;
    cfg.configPath = joinPath(dir, "config.json");
    cfg.uid = "agent-int";
    cfg.descr = "web-agent";
    cfg.serverUrl = "https://unused.test/api";
    cfg.accessCode = "";
    cfg.pollIntervalSec = 1;
    cfg.taskDirectory = joinPath(dir, "tasks");
    cfg.resultDirectory = joinPath(dir, "results");
    cfg.stateFile = joinPath(dir, "state.json");
    cfg.mockApiDirectory = joinPath(dir, "mock_api");
    cfg.maxCycles = 1;
    ensureDirectory(cfg.taskDirectory);
    ensureDirectory(cfg.resultDirectory);
    ensureDirectory(cfg.mockApiDirectory);
    writeTextFile(cfg.configPath, R"({"uid":"agent-int","server_url":"https://unused.test/api","max_cycles":1})");
    return cfg;
}
}  // namespace

TEST(AgentIntegrationTest, RegistrationTaskAndResultFlowWithMockApi) {
    auto cfg = makeMockConfig(test::makeTempDir("webagent_agent_flow"));
    writeTextFile(joinPath(cfg.mockApiDirectory, "register_response.json"),
                  R"({"code_responce":"0","msg":"ok","access_code":"acc-1"})");
    writeTextFile(joinPath(cfg.mockApiDirectory, "task_response.json"),
                  R"({"code_responce":"1","status":"ok","msg":"task","task_code":"TIMEOUT","options":"2","session_id":"sess-1"})");
    writeTextFile(joinPath(cfg.mockApiDirectory, "result_response.json"),
                  R"({"code_responce":"0","msg":"result accepted"})");

    Agent agent(cfg);
    const int rc = agent.run();
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(fileExists(cfg.stateFile));
}

TEST(AgentIntegrationTest, UnexpectedTaskResponseDoesNotCrashLoop) {
    auto cfg = makeMockConfig(test::makeTempDir("webagent_agent_unexpected"));
    cfg.accessCode = "acc-ready";
    writeTextFile(joinPath(cfg.mockApiDirectory, "task_response.json"),
                  R"({"code_responce":"unexpected","msg":"bad-shape"})");
    writeTextFile(joinPath(cfg.mockApiDirectory, "result_response.json"),
                  R"({"code_responce":"0"})");

    Agent agent(cfg);
    const int rc = agent.run();
    EXPECT_EQ(rc, 0);
}

}  // namespace webagent
