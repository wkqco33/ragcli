#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wlog.hpp>

#include "command.hpp"
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

        wcppcli::Flag model_flag;
        model_flag.name = "model";
        model_flag.shorthand = 'm';
        model_flag.description = "Ollama model name (default: llama3)";
        model_flag.value_ptr = &model_;
        cmd->add_flag(model_flag);

        wcppcli::Flag url_flag;
        url_flag.name = "url";
        url_flag.shorthand = 'u';
        url_flag.description = "Ollama base URL (default: http://localhost:11434)";
        url_flag.value_ptr = &base_url_;
        cmd->add_flag(url_flag);

        cmd->handler = [this](const wcppcli::Command &) { return run_chat(); };

        root.add_command(std::move(cmd));
    }

  private:
    auto run_chat() -> int {
        // WConf로 우선순위 설정 병합: CLI 플래그 > 환경 변수 > .env 파일 > 코드 기본값
        wcppcli::WConf conf;
        conf.read_file(".env");

        // CLI 플래그가 비어있으면 WConf 값을 사용
        std::string target_url = base_url_.empty() ? conf.get_string("OLLAMA_BASE_URL") : base_url_;
        std::string target_model = model_.empty() ? conf.get_string("OLLAMA_MODEL") : model_;

        wcppcli::WLog::info("Starting Ollama Chat Session");
        wcppcli::WLog::info("URL: " + target_url + " | Model: " + target_model);
        wcppcli::WLog::info("Type 'exit' or 'quit' to end chat.");

        try {
            auto client =
                llm_client::LLMClientFactory::create("ollama", /*api_key=*/"", target_url);
            llm_client::RequestParams params;
            params.model = target_model;

            std::vector<llm_client::Message> conversation;

            while (true) {
                std::cout << "\nYou: ";
                std::string input;
                if (!std::getline(std::cin, input)) {
                    wcppcli::WLog::info("Ending chat session.");
                    break;
                }

                if (input == "exit" || input == "quit") {
                    wcppcli::WLog::info("Ending chat session.");
                    break;
                }

                if (input.empty()) {
                    continue;
                }

                conversation.emplace_back("user", input);

                try {
                    auto response = client->chat(conversation, params);
                    wcppcli::WLog::success("Ollama: " + response.content);
                    conversation.emplace_back("assistant", response.content);
                } catch (const std::exception &e) {
                    wcppcli::WLog::error("Ollama Error: " + std::string(e.what()));
                    conversation.pop_back();
                }
            }
        } catch (const std::exception &e) {
            wcppcli::WLog::error("Failed to initialize Ollama client: " + std::string(e.what()));
            return 1;
        }

        return 0;
    }

    std::string model_;
    std::string base_url_;
};

} // namespace ragcli::cmd
