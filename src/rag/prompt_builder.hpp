#pragma once

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

// Qdrant 검색 결과를 바탕으로 LLM 질의용 프롬프트를 조립한다.
inline auto build_query_prompt(const std::vector<SearchHit> &hits, const std::string &query)
    -> std::string {
    std::string context;
    for (const auto &item : hits) {
        context += "[score: " + format_score(item.score) + "] " + item.text + "\n";
    }

    return std::string("아래 문서를 참고해서 질문에 답하세요. 문서에 관련 정보가 없으면 "
                       "'제공된 문서에는 답이 없습니다'라고 답하세요.\n\n문서:\n") +
           (context.empty() ? "(없음)\n" : context) + "\n질문: " + query;
}

} // namespace ragcli::rag
