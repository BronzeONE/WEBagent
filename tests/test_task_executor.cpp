#include "executor/task_executor.h"
#include "utils/file_utils.h"

#include "test_helpers.h"

#include <gtest/gtest.h>

namespace webagent {

namespace {
Config makeConfig(const std::string& dir) {
    Config cfg;
    cfg.configPath = joinPath(dir, "config.json");
    cfg.uid = "test-uid";
    cfg.serverUrl = "https://example.test/api";
    cfg.accessCode = "ac";
    cfg.taskDirectory = joinPath(dir, "tasks");
    cfg.resultDirectory = joinPath(dir, "results");
    cfg.stateFile = joinPath(dir, "state.json");
    writeTextFile(cfg.configPath, R"({"uid":"test-uid","server_url":"https://example.test/api"})");
    ensureDirectory(cfg.taskDirectory);
    ensureDirectory(cfg.resultDirectory);
    return cfg;
}
}  // namespace

TEST(TaskExecutorTest, ConfTaskSavesOptionsAndReturnsFile) {
    auto cfg = makeConfig(test::makeTempDir("webagent_exec_conf"));
    TaskExecutor executor(cfg);

    TaskDefinition task;
    task.type = TaskType::Config;
    task.taskCode = "CONF";
    task.sessionId = "s1";
    task.accessCode = "ac";
    task.options = "{\"k\":\"v\"}";

    const auto result = executor.execute(task);
    EXPECT_EQ(result.resultCode, 0);
    EXPECT_FALSE(result.files.empty());
    EXPECT_TRUE(fileExists(result.files.front()));
}

TEST(TaskExecutorTest, TimeoutTaskUpdatesPollInterval) {
    auto cfg = makeConfig(test::makeTempDir("webagent_exec_timeout"));
    TaskExecutor executor(cfg);

    TaskDefinition task;
    task.type = TaskType::Timeout;
    task.taskCode = "TIMEOUT";
    task.sessionId = "s2";
    task.accessCode = "ac";
    task.options = "15";

    const auto result = executor.execute(task);
    EXPECT_EQ(result.resultCode, 0);
    EXPECT_EQ(cfg.pollIntervalSec, 15);
}

TEST(TaskExecutorTest, FileTaskCreatesFallbackFile) {
    auto cfg = makeConfig(test::makeTempDir("webagent_exec_file"));
    TaskExecutor executor(cfg);

    TaskDefinition task;
    task.type = TaskType::File;
    task.taskCode = "FILE";
    task.sessionId = "s3";
    task.accessCode = "ac";
    task.options = "non-existing.txt";

    const auto result = executor.execute(task);
    EXPECT_EQ(result.resultCode, 0);
    ASSERT_EQ(result.files.size(), 1u);
    EXPECT_TRUE(fileExists(result.files[0]));
}

TEST(TaskExecutorTest, TaskTaskHandlesSuccessAndFailure) {
    auto cfg = makeConfig(test::makeTempDir("webagent_exec_task"));
    TaskExecutor executor(cfg);

    TaskDefinition okTask;
    okTask.type = TaskType::Task;
    okTask.taskCode = "TASK";
    okTask.sessionId = "s4";
    okTask.accessCode = "ac";
    okTask.options = "echo unit-ok";
    const auto okResult = executor.execute(okTask);
    EXPECT_EQ(okResult.resultCode, 0);
    EXPECT_FALSE(okResult.files.empty());

    TaskDefinition failTask = okTask;
    failTask.sessionId = "s5";
    failTask.options = "false";
    const auto failResult = executor.execute(failTask);
    EXPECT_EQ(failResult.resultCode, -1);
    EXPECT_NE(failResult.message.find("failed"), std::string::npos);
}

}  // namespace webagent
