#pragma once

#include <string>

namespace ragcli::rag {

// RAG 실행에 필요한 모든 대상 설정을 묶는 구조체.
struct RagTargets {
    std::string llm_url;
    std::string model;
    std::string embed_model;
    std::string qdrant_url;
    std::string collection;
    std::string provider;     // "ollama"(기본값), "openai", "azure"
    std::string api_key;      // ollama 는 항상 빈 문자열
    std::string api_version;  // azure 전용
};

// CLI > env > fallback 우선순위로 설정값을 선택하기 위한 입력.
struct ConfigSource {
    const std::string *cli = nullptr;
    std::string env;
    std::string fallback;
};

// CLI > env > fallback 우선순위로 설정값을 선택한다.
inline auto pick_first(const ConfigSource &src) -> std::string {
    if (src.cli != nullptr && !src.cli->empty()) {
        return *src.cli;
    }
    if (!src.env.empty()) {
        return src.env;
    }
    return src.fallback;
}

// CLI > env > fallback 우선순위로 양의 정수 설정값을 선택한다.
// 0 이하인 값은 "설정되지 않음"으로 취급한다 (wcppcli::WConf::get_int() 는
// 키가 없으면 0 을 반환하므로 그대로 이 규칙과 맞아떨어진다).
inline auto pick_first_positive_int(int cli, int env, int fallback) -> int {
    if (cli > 0) {
        return cli;
    }
    if (env > 0) {
        return env;
    }
    return fallback;
}

} // namespace ragcli::rag
