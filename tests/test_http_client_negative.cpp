#include "executor/task_executor.h"
#include "network/http_client.h"
#include "utils/file_utils.h"

#include "test_helpers.h"

#include <gtest/gtest.h>

namespace webagent {

namespace {
Config makeHttpConfig(const std::string& dir) {
    Config cfg;
    cfg.uid = "http-test";
    cfg.serverUrl = "https://127.0.0.1:1";
    cfg.connectTimeoutSec = 1;
    cfg.requestTimeoutSec = 1;
    cfg.taskDirectory = joinPath(dir, "tasks");
    cfg.resultDirectory = joinPath(dir, "results");
    cfg.configPath = joinPath(dir, "config.json");
    writeTextFile(cfg.configPath, R"({"uid":"http-test","server_url":"https://127.0.0.1:1"})");
    ensureDirectory(cfg.taskDirectory);
    ensureDirectory(cfg.resultDirectory);
    return cfg;
}
}  // namespace

TEST(HttpClientNegativeTest, ThrowsWhenServerUnavailable) {
    const auto cfg = makeHttpConfig(test::makeTempDir("webagent_http_unavailable"));
    HttpClient client(cfg);
    EXPECT_THROW(client.postJson(cfg.serverUrl + "/wa_task/", R"({"a":1})", "task"), std::runtime_error);
}

TEST(HttpClientNegativeTest, MultipartThrowsWhenServerUnavailable) {
    const auto cfg = makeHttpConfig(test::makeTempDir("webagent_http_multipart"));
    HttpClient client(cfg);
    EXPECT_THROW(client.postMultipartResult(cfg.serverUrl + "/wa_result/", 0, R"({"UID":"u"})", {}),
                 std::runtime_error);
}

TEST(SecurityBehaviorTest, ConfigAllowListWorksForMatching) {
    Config cfg;
    cfg.allowedPrograms = {"echo", "ls"};
    EXPECT_TRUE(cfg.isProgramAllowed("echo"));
    EXPECT_FALSE(cfg.isProgramAllowed("cat"));
}

TEST(SecurityBehaviorTest, TaskExecutorShouldEnforceAllowList) {
    auto cfg = makeHttpConfig(test::makeTempDir("webagent_allowlist_todo"));
    cfg.allowedPrograms = {"echo"};
    TaskExecutor executor(cfg);

    TaskDefinition task;
    task.type = TaskType::Task;
    task.taskCode = "TASK";
    task.sessionId = "sess";
    task.accessCode = "acc";
    task.options = "cat /etc/hosts";

    const auto result = executor.execute(task);
    EXPECT_EQ(result.resultCode, -1);
}

}  // namespace webagent
