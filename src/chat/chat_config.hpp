#pragma once

#include <string>
#include <wcppcli/wconf.hpp>

#include "llm/provider_config.hpp"

namespace ragcli::chat {

// chat 모드에서 필요한 LLM 대상 설정.
struct ChatTargets {
    std::string url;
    std::string model;
    std::string provider;
    std::string api_key;
    std::string api_version;
};

// CLI > env > fallback 우선순위로 chat 설정값을 선택한다.
inline auto resolve_chat_targets(const std::string *cli_url, const std::string *cli_model,
                                 const std::string *cli_provider,
                                 const wcppcli::WConf &conf) -> ChatTargets {
    llm::ProviderOverrides overrides{cli_provider, cli_url, cli_model, /*embed_model=*/nullptr};
    llm::ProviderTargets resolved = llm::resolve_provider_targets(overrides, conf);

    ChatTargets targets;
    targets.provider = resolved.provider;
    targets.url = resolved.base_url;
    targets.model = resolved.model;
    targets.api_key = resolved.api_key;
    targets.api_version = resolved.api_version;
    return targets;
}

} // namespace ragcli::chat
