#pragma once

#include <string>
#include <wcppcli/wconf.hpp>

namespace ragcli::chat {

// chat 모드에서 필요한 LLM 대상 설정.
struct ChatTargets {
    std::string url;
    std::string model;
};

// CLI > env > fallback 우선순위로 chat 설정값을 선택한다.
inline auto resolve_chat_targets(const std::string *cli_url, const std::string *cli_model,
                                 const wcppcli::WConf &conf) -> ChatTargets {
    ChatTargets targets;
    targets.url =
        (cli_url != nullptr && !cli_url->empty()) ? *cli_url : conf.get_string("OLLAMA_BASE_URL");
    targets.model = (cli_model != nullptr && !cli_model->empty()) ? *cli_model
                                                                  : conf.get_string("OLLAMA_MODEL");
    return targets;
}

} // namespace ragcli::chat
