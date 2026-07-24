#pragma once

#include <string>
#include <wcppcli/wconf.hpp>

namespace ragcli::llm {

// `rag`/`chat` 커맨드가 공유하는 LLM 프로바이더 설정.
// 지원 프로바이더: "ollama"(기본값), "openai", "azure".
// (llm_client 라이브러리 자체가 anthropic/gemini 를 지원하지 않아 제외됨)
struct ProviderTargets {
    std::string provider;
    std::string base_url;
    std::string model;
    std::string embed_model;
    std::string api_key;     // ollama 는 항상 빈 문자열
    std::string api_version; // azure 전용, 비어 있으면 llm_client 기본값 사용
};

// CLI 오버라이드. embed_model 은 chat 커맨드처럼 임베딩을 쓰지 않는 경우
// nullptr 로 두면 된다.
struct ProviderOverrides {
    const std::string *provider = nullptr;
    const std::string *base_url = nullptr;
    const std::string *model = nullptr;
    const std::string *embed_model = nullptr;
};

namespace detail {
inline auto pick(const std::string *cli, const std::string &env, const std::string &fallback)
    -> std::string {
    if (cli != nullptr && !cli->empty()) {
        return *cli;
    }
    if (!env.empty()) {
        return env;
    }
    return fallback;
}
} // namespace detail

// CLI 플래그 > 환경 변수(.env) > 코드 기본값 순으로 LLM 프로바이더 설정을 해석한다.
inline auto resolve_provider_targets(const ProviderOverrides &overrides, const wcppcli::WConf &conf)
    -> ProviderTargets {
    ProviderTargets targets;
    targets.provider = detail::pick(overrides.provider, conf.get_string("LLM_PROVIDER"), "ollama");

    if (targets.provider == "openai") {
        targets.base_url = detail::pick(overrides.base_url, conf.get_string("OPENAI_BASE_URL"), "");
        targets.model =
            detail::pick(overrides.model, conf.get_string("OPENAI_MODEL"), "gpt-4o-mini");
        targets.embed_model = detail::pick(overrides.embed_model, conf.get_string("OPENAI_EMBED_MODEL"),
                                           "text-embedding-3-small");
        targets.api_key = conf.get_string("OPENAI_API_KEY");
    } else if (targets.provider == "azure") {
        targets.base_url =
            detail::pick(overrides.base_url, conf.get_string("AZURE_OPENAI_BASE_URL"), "");
        targets.model = detail::pick(overrides.model, conf.get_string("AZURE_OPENAI_MODEL"), "");
        targets.embed_model =
            detail::pick(overrides.embed_model, conf.get_string("AZURE_OPENAI_EMBED_MODEL"), "");
        targets.api_key = conf.get_string("AZURE_OPENAI_API_KEY");
        targets.api_version = conf.get_string("AZURE_OPENAI_API_VERSION");
    } else {
        // "ollama"(기본값) 및 인식되지 않는 값은 그대로 llm_client 로 넘겨
        // 프로바이더 검증/에러 처리를 위임한다.
        targets.base_url = detail::pick(overrides.base_url, conf.get_string("OLLAMA_BASE_URL"),
                                        "http://localhost:11434");
        targets.model = detail::pick(overrides.model, conf.get_string("OLLAMA_MODEL"), "llama3");
        targets.embed_model = detail::pick(overrides.embed_model, conf.get_string("OLLAMA_EMBED_MODEL"),
                                           "nomic-embed-text");
    }

    return targets;
}

} // namespace ragcli::llm
