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

} // namespace ragcli::rag
