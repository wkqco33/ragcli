#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <wcppcli/wlog.hpp>

#include "chat/chat_config.hpp"
#include "llm_client/llm_client_interface.hpp"

namespace ragcli::chat {

// 대화 상태 변화를 외부로 알리는 콜백.
struct ChatCallbacks {
    // 사용자 입력을 요청한다. false 를 반환하면 입력 종료로 간주한다.
    std::function<bool(std::string &out)> read_input;

    // 새 사용자 메시지를 출력한다 (예: "You: ").
    std::function<void()> prompt_user;

    // assistant 응답을 출력한다.
    std::function<void(const std::string &)> on_assistant_reply;

    // LLM API 오류 발생 시 호출된다.
    std::function<void(const std::string &)> on_error;
};

// `ragcli chat` 의 인터랙티브 대화 흐름을 담당한다.
// LLM 클라이언트는 생성자로 주입받는다.
class ChatRunner {
  public:
    explicit ChatRunner(std::shared_ptr<llm_client::LLMClientInterface> llm_client)
        : llm_client_(std::move(llm_client)) {}

    // 한 번의 대화 세션을 실행한다.
    // callbacks 가 비어있으면 기본 콘솔 동작을 사용한다.
    auto run(const ChatTargets &targets, ChatCallbacks callbacks) -> int {
        auto read_input_fn = callbacks.read_input
                                 ? std::move(callbacks.read_input)
                                 : std::function<bool(std::string &)>(default_read_input);
        auto prompt_user_fn = callbacks.prompt_user ? std::move(callbacks.prompt_user)
                                                    : std::function<void()>(default_prompt_user);
        auto on_assistant_reply_fn =
            callbacks.on_assistant_reply
                ? std::move(callbacks.on_assistant_reply)
                : std::function<void(const std::string &)>(default_on_assistant_reply);
        auto on_error_fn = callbacks.on_error
                               ? std::move(callbacks.on_error)
                               : std::function<void(const std::string &)>(default_on_error);

        try {
            llm_client::RequestParams params;
            params.model = targets.model;

            std::vector<llm_client::Message> conversation;

            while (true) {
                prompt_user_fn();

                std::string input;
                const bool should_continue = read_input_fn(input);

                if (!should_continue) {
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
                    auto response = llm_client_->chat(conversation, params);
                    on_assistant_reply_fn(response.content);
                    conversation.emplace_back("assistant", response.content);
                } catch (const std::exception &e) {
                    on_error_fn(std::string("Ollama Error: ") + e.what());
                    conversation.pop_back();
                }
            }
        } catch (const std::exception &e) {
            wcppcli::WLog::error("Failed to run chat session: " + std::string(e.what()));
            return 1;
        }

        return 0;
    }

  private:
    static auto default_read_input(std::string &out) -> bool {
        return static_cast<bool>(std::getline(std::cin, out));
    }

    static void default_prompt_user() {
        std::cout << "\nYou: ";
    }

    static void default_on_assistant_reply(const std::string &content) {
        wcppcli::WLog::success("Ollama: " + content);
    }

    static void default_on_error(const std::string &message) {
        wcppcli::WLog::error(message);
    }

    std::shared_ptr<llm_client::LLMClientInterface> llm_client_;
};

} // namespace ragcli::chat
