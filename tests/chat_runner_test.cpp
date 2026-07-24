#include <memory>
#include <queue>
#include <string>

#include <gtest/gtest.h>

#include "chat/chat_config.hpp"
#include "chat/chat_runner.hpp"

#include "chat_runner_test.hpp"

using ragcli::test::make_input_provider;
using ragcli::test::MockLlmClient;
using ragcli::test::MockLlmClientWithError;

TEST(ChatRunner, EchoesAssistantReplyFromMock) {
    auto llm_client = std::make_shared<MockLlmClient>();
    ragcli::chat::ChatRunner runner(llm_client);

    ragcli::chat::ChatTargets targets;
    targets.url = "http://localhost:11434";
    targets.model = "mock-model";

    std::queue<std::string> inputs;
    inputs.push("hello");
    inputs.push("quit");
    bool eof_reached = false;

    std::vector<std::string> replies;

    ragcli::chat::ChatCallbacks callbacks;
    callbacks.read_input = make_input_provider(inputs, &eof_reached);
    callbacks.prompt_user = []() {};
    callbacks.on_assistant_chunk = [](const std::string &) {};
    callbacks.on_assistant_reply = [&replies](const std::string &content) {
        replies.push_back(content);
    };
    callbacks.on_error = [](const std::string & /*message*/) {};

    const int exit_code = runner.run(targets, std::move(callbacks));

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(replies.size(), 1U);
    EXPECT_EQ(replies[0], "mock reply to: hello");
    EXPECT_FALSE(eof_reached); // "quit" 으로 정상 종료됨
}

TEST(ChatRunner, EofEndsSession) {
    auto llm_client = std::make_shared<MockLlmClient>();
    ragcli::chat::ChatRunner runner(llm_client);

    ragcli::chat::ChatTargets targets;
    targets.url = "http://localhost:11434";
    targets.model = "mock-model";

    std::queue<std::string> inputs;
    inputs.push("hello");
    bool eof_reached = false;

    std::vector<std::string> replies;

    ragcli::chat::ChatCallbacks callbacks;
    callbacks.read_input = make_input_provider(inputs, &eof_reached);
    callbacks.prompt_user = []() {};
    callbacks.on_assistant_chunk = [](const std::string &) {};
    callbacks.on_assistant_reply = [&replies](const std::string &content) {
        replies.push_back(content);
    };
    callbacks.on_error = [](const std::string & /*message*/) {};

    const int exit_code = runner.run(targets, std::move(callbacks));

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(replies.size(), 1U);
    EXPECT_TRUE(eof_reached); // EOF 로 종료됨
}

TEST(ChatRunner, IgnoresEmptyInput) {
    auto llm_client = std::make_shared<MockLlmClient>();
    ragcli::chat::ChatRunner runner(llm_client);

    ragcli::chat::ChatTargets targets;
    targets.url = "http://localhost:11434";
    targets.model = "mock-model";

    std::queue<std::string> inputs;
    inputs.push("");
    inputs.push("say something");
    inputs.push("exit");

    std::vector<std::string> replies;

    ragcli::chat::ChatCallbacks callbacks;
    callbacks.read_input = make_input_provider(inputs);
    callbacks.prompt_user = []() {};
    callbacks.on_assistant_chunk = [](const std::string &) {};
    callbacks.on_assistant_reply = [&replies](const std::string &content) {
        replies.push_back(content);
    };
    callbacks.on_error = [](const std::string & /*message*/) {};

    const int exit_code = runner.run(targets, std::move(callbacks));

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(replies.size(), 1U);
}

TEST(ChatRunner, KeepsConversationHistory) {
    auto llm_client = std::make_shared<MockLlmClient>();
    ragcli::chat::ChatRunner runner(llm_client);

    ragcli::chat::ChatTargets targets;
    targets.url = "http://localhost:11434";
    targets.model = "mock-model";

    std::queue<std::string> inputs;
    inputs.push("first");
    inputs.push("second");
    inputs.push("quit");

    ragcli::chat::ChatCallbacks callbacks;
    callbacks.read_input = make_input_provider(inputs);
    callbacks.prompt_user = []() {};
    callbacks.on_assistant_chunk = [](const std::string &) {};
    callbacks.on_assistant_reply = [](const std::string & /*content*/) {};
    callbacks.on_error = [](const std::string & /*message*/) {};

    runner.run(targets, std::move(callbacks));

    // 종료 시점의 messages: user first, assistant first, user second, assistant second
    ASSERT_GE(llm_client->last_messages_.size(), 3U);
    EXPECT_EQ(llm_client->last_messages_[0].role, "user");
    EXPECT_EQ(llm_client->last_messages_[0].content, "first");
    EXPECT_EQ(llm_client->last_messages_[1].role, "assistant");
    EXPECT_EQ(llm_client->last_messages_[2].role, "user");
    EXPECT_EQ(llm_client->last_messages_[2].content, "second");
}

TEST(ChatRunner, ErrorRollsBackUserMessage) {
    auto llm_client = std::make_shared<MockLlmClientWithError>();
    ragcli::chat::ChatRunner runner(llm_client);

    ragcli::chat::ChatTargets targets;
    targets.url = "http://localhost:11434";
    targets.model = "throw";

    std::queue<std::string> inputs;
    inputs.push("trigger error");
    inputs.push("quit");

    std::vector<std::string> errors;

    ragcli::chat::ChatCallbacks callbacks;
    callbacks.read_input = make_input_provider(inputs);
    callbacks.prompt_user = []() {};
    callbacks.on_assistant_chunk = [](const std::string &) {};
    callbacks.on_assistant_reply = [](const std::string & /*content*/) {};
    callbacks.on_error = [&errors](const std::string &message) { errors.push_back(message); };

    runner.run(targets, std::move(callbacks));

    ASSERT_EQ(errors.size(), 1U);
    EXPECT_NE(errors[0].find("forced error"), std::string::npos);

    // 에러 후 롤백되어 마지막 사용자 메시지가 history 에 남지 않아야 한다.
    EXPECT_TRUE(llm_client->last_messages_.empty());
}
