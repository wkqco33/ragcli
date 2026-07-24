#pragma once

#include <string>

namespace ragcli::indexing {

// 인덱싱 작업 옵션.
struct IndexOptions {
    std::string embed_model;            // 사용할 임베딩 모델
    std::string source_type;            // Qdrant payload source_type (기본: ragcli_index)
    std::string distance = "Cosine";    // Qdrant 거리 함수
    bool auto_create_collection = true; // 컬렉션이 없으면 자동 생성
    bool skip_empty = true;             // 빈 텍스트 페이지를 건너뛸지 여부
};

} // namespace ragcli::indexing
