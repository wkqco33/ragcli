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
#include "utils/config_path.hpp"

namespace ragcli::cmd {

// `ragcli chat` 을 처리하는 서브커맨드.
// Ollama LLM 서버와 연동하여 인터랙티브 CLI 대화를 수행한다.
class ChatCommand : public CommandBase {
  public:
    void register_to(wcppcli::Command &root) override {
        auto cmd = std::make_unique<wcppcli::Command>();
        cmd->name = "chat";
        cmd->description = "Start an interactive chat session using an LLM provider";

        add_string_flag(*cmd, "provider", 0,
                        "LLM provider: 'ollama' (default), 'openai', or 'azure'", &provider_);
        add_string_flag(*cmd, "model", 'm', "LLM model name (default depends on --provider)",
                        &model_);
        add_string_flag(*cmd, "url", 'u',
                        "LLM base URL (default depends on --provider, e.g. "
                        "http://localhost:11434 for ollama)",
                        &base_url_);

        cmd->handler = [this](const wcppcli::Command & /*unused*/) { return run_chat(); };

        root.add_command(std::move(cmd));
    }

  private:
    auto run_chat() -> int {
        wcppcli::WConf conf;
        ragcli::utils::load_config(conf);

        const ragcli::chat::ChatTargets targets =
            ragcli::chat::resolve_chat_targets(&base_url_, &model_, &provider_, conf);

        wcppcli::WLog::info("Starting " + targets.provider + " Chat Session");
        wcppcli::WLog::info("URL: " + targets.url + " | Model: " + targets.model);
        wcppcli::WLog::info("Type 'exit' or 'quit' to end chat.");

        auto llm_client = llm_client::LLMClientFactory::create(targets.provider, targets.api_key,
                                                               targets.url, targets.api_version);
        ragcli::chat::ChatRunner runner(std::move(llm_client));

        return runner.run(targets, {});
    }

    std::string model_;
    std::string base_url_;
    std::string provider_;
};

} // namespace ragcli::cmd
