#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wlog.hpp>

#include "chat/chat_config.hpp"
#include "chat/chat_runner.hpp"
#include "command.hpp"
#include "flag_helper.hpp"
#include "llm_client/llm_client_factory.hpp"
#include "wcppcli/wconf.hpp"

namespace ragcli::cmd {

// `ragcli chat` 을 처리하는 서브커맨드.
// Ollama LLM 서버와 연동하여 인터랙티브 CLI 대화를 수행한다.
class ChatCommand : public CommandBase {
  public:
    void register_to(wcppcli::Command &root) override {
        auto cmd = std::make_unique<wcppcli::Command>();
        cmd->name = "chat";
        cmd->description = "Start an interactive chat session using Ollama LLM";

        add_string_flag(*cmd, "model", 'm', "Ollama model name (default: llama3)", &model_);
        add_string_flag(*cmd, "url", 'u', "Ollama base URL (default: http://localhost:11434)",
                        &base_url_);

        cmd->handler = [this](const wcppcli::Command & /*unused*/) { return run_chat(); };

        root.add_command(std::move(cmd));
    }

  private:
    auto run_chat() -> int {
        wcppcli::WConf conf;
        conf.read_file(".env");

        const ragcli::chat::ChatTargets targets =
            ragcli::chat::resolve_chat_targets(&base_url_, &model_, conf);

        wcppcli::WLog::info("Starting Ollama Chat Session");
        wcppcli::WLog::info("URL: " + targets.url + " | Model: " + targets.model);
        wcppcli::WLog::info("Type 'exit' or 'quit' to end chat.");

        auto llm_client =
            llm_client::LLMClientFactory::create("ollama", /*api_key=*/"", targets.url);
        ragcli::chat::ChatRunner runner(std::move(llm_client));

        return runner.run(targets, {});
    }

    std::string model_;
    std::string base_url_;
};

} // namespace ragcli::cmd
