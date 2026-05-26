#include "network/api_types.h"
#include "utils/json.h"

#include <gtest/gtest.h>

namespace webagent {

TEST(ApiTypesTest, ParsesValidWaTaskResponse) {
    const auto json = Json::parse(
        R"({"code_responce":"1","status":"ok","msg":"task","task_code":"TASK","options":"echo hi","session_id":"s-1"})");
    const auto response = parseTaskPollResponse(json);

    EXPECT_EQ(response.codeResponse, "1");
    EXPECT_EQ(response.task.taskCode, "TASK");
    EXPECT_EQ(response.task.type, TaskType::Task);
    EXPECT_EQ(response.task.options, "echo hi");
    EXPECT_EQ(response.task.sessionId, "s-1");
}

TEST(ApiTypesTest, EmptyJsonUsesFallbacks) {
    const auto response = parseTaskPollResponse(Json::object{});
    EXPECT_TRUE(response.codeResponse.empty());
    EXPECT_EQ(response.task.type, TaskType::Unknown);
    EXPECT_TRUE(response.task.taskCode.empty());
}

TEST(ApiTypesTest, InvalidJsonThrows) {
    EXPECT_THROW(Json::parse("{"), std::runtime_error);
    EXPECT_THROW(Json::parse("not-json"), std::runtime_error);
}

TEST(ApiTypesTest, MissingRequiredTaskFieldsDoNotCrash) {
    const auto json = Json::parse(R"({"code_responce":"1","status":"ok"})");
    const auto response = parseTaskPollResponse(json);
    EXPECT_EQ(response.codeResponse, "1");
    EXPECT_TRUE(response.task.taskCode.empty());
    EXPECT_EQ(response.task.type, TaskType::Unknown);
    EXPECT_TRUE(response.task.sessionId.empty());
}

TEST(ApiTypesTest, ParsesNewApiTaskArrayFormat) {
    const auto json = Json::parse(
        R"({"code_responce":"1","tasks":[{"id":"t1","type":"run_command","command":"echo ok","session_id":"s-array"}]})");
    const auto response = parseTaskPollResponse(json);
    ASSERT_EQ(response.tasks.size(), 1u);
    EXPECT_EQ(response.tasks[0].id, "t1");
    EXPECT_EQ(response.tasks[0].type, TaskType::RunCommand);
    EXPECT_EQ(response.tasks[0].options, "echo ok");
}

}  // namespace webagent
