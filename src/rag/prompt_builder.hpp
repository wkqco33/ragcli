#pragma once

#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "rag/qdrant_port.hpp"

namespace ragcli::rag {

// 검색 점수를 고정 소수점 4자리 문자열로 포맷팅한다.
inline auto format_score(double score) -> std::string {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(4);
    oss << score;
    return oss.str();
}

// XML 속성 값에 쓰기 위해 최소한의 특수문자를 이스케이프한다. '>'는 속성 값
// 안에서 이스케이프가 필수는 아니고, heading_path 의 " > " 구분자를 그대로
// 읽기 쉽게 남겨두기 위해 escape 대상에서 제외한다.
inline auto escape_attr(const std::string &value) -> std::string {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '<':
            out += "&lt;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

// 히트 한 건을 <doc id="N" source="..." section="..." page="N" chunk="i/total"
// score="0.xxxx">...</doc> 블록으로 렌더링한다. 값이 없는 속성은 생략한다.
inline auto render_doc_block(std::size_t index, const SearchHit &hit) -> std::string {
    std::string attrs = " id=\"" + std::to_string(index) + "\"";
    if (!hit.source.empty()) {
        attrs += " source=\"" + escape_attr(hit.source) + "\"";
    }
    if (!hit.heading_path.empty()) {
        attrs += " section=\"" + escape_attr(hit.heading_path) + "\"";
    }
    if (hit.page_index > 0) {
        attrs += " page=\"" + std::to_string(hit.page_index) + "\"";
    }
    if (hit.chunk_total > 0) {
        attrs += " chunk=\"" + std::to_string(hit.chunk_index + 1) + "/" +
                 std::to_string(hit.chunk_total) + "\"";
    }
    attrs += " score=\"" + format_score(hit.score) + "\"";

    const std::string body = hit.is_image ? "[이미지 — 아래 첨부 참조]" : hit.text;

    return "<doc" + attrs + ">\n" + body + "\n</doc>\n";
}

// Qdrant 검색 결과를 바탕으로 LLM 질의용 프롬프트를 조립한다. 각 조각을
// 구조화된 <doc> 블록으로 감싸 출처/섹션/페이지를 드러내고, 문장 단위 인용을
// 지시해 LLM 이 검색 결과를 근거로 답을 구성하고 출처를 밝힐 수 있게 한다.
inline auto build_query_prompt(const std::vector<SearchHit> &hits,
                               const std::string &query) -> std::string {
    std::string context;
    for (std::size_t i = 0; i < hits.size(); ++i) {
        context += render_doc_block(i + 1, hits[i]);
    }

    return std::string("아래 문서 조각들을 근거로 질문에 답하세요.\n"
                       "- 답변에 사용한 근거는 문장 끝에 [1], [2] 형식으로 인용하세요 (문서의 id 사용).\n"
                       "- 조각에 근거가 없으면 '제공된 문서에는 답이 없습니다'라고만 답하세요.\n"
                       "- 조각끼리 내용이 상충하면 그 사실을 명시하세요.\n\n") +
           (context.empty() ? "(문서 없음)\n" : context) + "\n질문: " + query;
}

} // namespace ragcli::rag
