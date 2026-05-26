#include "core/config.h"
#include "utils/file_utils.h"

#include "test_helpers.h"

#include <gtest/gtest.h>

namespace webagent {

TEST(ConfigTest, LoadsDefaultsWhenOptionalFieldsAbsent) {
    const auto dir = test::makeTempDir("webagent_config_defaults");
    const auto configPath = joinPath(dir, "config.json");
    writeTextFile(configPath, R"({"uid":"u1","server_url":"https://example.test/api"})");

    const auto cfg = Config::loadFromFile(configPath);

    EXPECT_EQ(cfg.descr, "web-agent");
    EXPECT_EQ(cfg.pollIntervalSec, 10);
    EXPECT_EQ(cfg.taskDirectory, "./tasks");
    EXPECT_EQ(cfg.resultDirectory, "./results");
    EXPECT_EQ(cfg.requestTimeoutSec, 30);
}

TEST(ConfigTest, LoadsPartiallyFilledConfig) {
    const auto dir = test::makeTempDir("webagent_config_partial");
    const auto configPath = joinPath(dir, "config.json");
    writeTextFile(
        configPath,
        R"({"uid":"u2","server_url":"https://example.test/api","poll_interval_sec":3,"allowed_programs":["echo"],"mock_api_directory":"./mock"})");

    const auto cfg = Config::loadFromFile(configPath);

    EXPECT_EQ(cfg.uid, "u2");
    EXPECT_EQ(cfg.pollIntervalSec, 3);
    ASSERT_EQ(cfg.allowedPrograms.size(), 1u);
    EXPECT_EQ(cfg.allowedPrograms[0], "echo");
    EXPECT_TRUE(cfg.isMockMode());
}

TEST(ConfigTest, SaveConfigAndStateRoundTrip) {
    const auto dir = test::makeTempDir("webagent_config_roundtrip");
    const auto configPath = joinPath(dir, "config.json");
    const auto statePath = joinPath(dir, "state.json");

    writeTextFile(configPath, R"({"uid":"u3","server_url":"https://example.test/api","state_file":"./agent_state.json"})");
    auto cfg = Config::loadFromFile(configPath);
    cfg.configPath = configPath;
    cfg.stateFile = statePath;
    cfg.accessCode = "acc-123";
    cfg.pollIntervalSec = 7;
    cfg.lastServerTaskCode = "TIMEOUT";
    cfg.lastServerOptions = "7";
    cfg.lastChangedFields = {"poll_interval_sec"};

    cfg.saveConfig();
    cfg.saveState();

    const auto loaded = Config::loadFromFile(configPath);
    EXPECT_EQ(loaded.uid, "u3");
    EXPECT_EQ(loaded.pollIntervalSec, 7);
    EXPECT_EQ(loaded.accessCode, "acc-123");
    EXPECT_EQ(loaded.lastServerTaskCode, "TIMEOUT");
}

}  // namespace webagent
