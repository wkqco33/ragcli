#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "rag/qdrant_port.hpp"
#include "utils/utf8.hpp"

namespace ragcli::rag {

namespace lexical_detail {

inline auto is_ascii_alnum(char32_t cp) -> bool {
    return (cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z');
}

inline auto to_ascii_lower(char32_t cp) -> char32_t {
    if (cp >= 'A' && cp <= 'Z') {
        return cp + (static_cast<char32_t>('a') - static_cast<char32_t>('A'));
    }
    return cp;
}

// 한글/CJK 판별 (음절/자모/한자/가나 범위). 이 범위의 문자는 공백 없이 이어지는
// 경우가 흔해 단어 단위 토큰화가 불가능하므로 문자 bigram 으로 처리한다.
inline auto is_cjk(char32_t cp) -> bool {
    return (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0x1100 && cp <= 0x11FF) ||
           (cp >= 0x3130 && cp <= 0x318F) || (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0x3040 && cp <= 0x30FF);
}

inline auto encode_utf8(char32_t cp) -> std::string {
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

} // namespace lexical_detail

// 텍스트를 BM25 채점용 토큰으로 분해한다. ASCII 영숫자는 소문자화한 단어
// 토큰으로, 한글/CJK 는 공백 유무와 무관하게 매칭되도록 문자 2-gram 으로
// 쪼갠다 (단일 글자만 있으면 유니그램 그대로 보존).
inline auto tokenize(const std::string &text) -> std::vector<std::string> {
    using namespace lexical_detail;

    std::vector<std::string> tokens;
    std::string word_buf;
    std::vector<char32_t> cjk_run;

    auto flush_word = [&]() {
        if (!word_buf.empty()) {
            tokens.push_back(word_buf);
            word_buf.clear();
        }
    };
    auto flush_cjk = [&]() {
        if (cjk_run.size() == 1) {
            tokens.push_back(encode_utf8(cjk_run[0]));
        } else {
            for (std::size_t i = 0; i + 1 < cjk_run.size(); ++i) {
                tokens.push_back(encode_utf8(cjk_run[i]) + encode_utf8(cjk_run[i + 1]));
            }
        }
        cjk_run.clear();
    };

    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t len = 0;
        const char32_t cp = utils::utf8::decode_codepoint(text, pos, len);

        if (is_ascii_alnum(cp)) {
            flush_cjk();
            word_buf += static_cast<char>(to_ascii_lower(cp));
        } else if (is_cjk(cp)) {
            flush_word();
            cjk_run.push_back(cp);
        } else {
            flush_word();
            flush_cjk();
        }
        pos += len;
    }
    flush_word();
    flush_cjk();

    return tokens;
}

struct Bm25Params {
    double k1 = 1.2;
    double b = 0.75;
};

// query_tokens 와 doc_tokens[i] 사이의 BM25 점수를 계산한다. IDF/평균 문서
// 길이는 doc_tokens 전체(=후보 풀)로부터 즉석에서 구한 근사치다 — 컬렉션
// 전체 통계가 없으므로 검색 후보군 규모에서만 유효한 상대 점수다.
inline auto bm25_scores(const std::vector<std::string> &query_tokens,
                        const std::vector<std::vector<std::string>> &doc_tokens,
                        const Bm25Params &params = {}) -> std::vector<double> {
    const std::size_t n_docs = doc_tokens.size();
    std::vector<double> scores(n_docs, 0.0);
    if (n_docs == 0 || query_tokens.empty()) {
        return scores;
    }

    std::vector<std::size_t> doc_len(n_docs, 0);
    std::unordered_map<std::string, int> doc_freq;
    double total_len = 0.0;

    for (std::size_t i = 0; i < n_docs; ++i) {
        doc_len[i] = doc_tokens[i].size();
        total_len += static_cast<double>(doc_len[i]);

        std::unordered_set<std::string> seen;
        for (const auto &tok : doc_tokens[i]) {
            if (seen.insert(tok).second) {
                ++doc_freq[tok];
            }
        }
    }
    const double avg_len = total_len / static_cast<double>(n_docs);

    const std::unordered_set<std::string> query_set(query_tokens.begin(), query_tokens.end());

    for (std::size_t i = 0; i < n_docs; ++i) {
        std::unordered_map<std::string, int> term_freq;
        for (const auto &tok : doc_tokens[i]) {
            ++term_freq[tok];
        }

        double score = 0.0;
        for (const auto &qterm : query_set) {
            const auto tf_it = term_freq.find(qterm);
            if (tf_it == term_freq.end()) {
                continue;
            }
            const auto df_it = doc_freq.find(qterm);
            const int df = df_it != doc_freq.end() ? df_it->second : 0;
            const double idf =
                std::log(1.0 + (static_cast<double>(n_docs) - df + 0.5) / (df + 0.5));
            const double tf = static_cast<double>(tf_it->second);
            const double len_norm =
                avg_len > 0.0 ? static_cast<double>(doc_len[i]) / avg_len : 1.0;
            const double denom = tf + params.k1 * (1.0 - params.b + params.b * len_norm);
            score += idf * (tf * (params.k1 + 1.0)) / denom;
        }
        scores[i] = score;
    }

    return scores;
}

// 벡터 검색 순위(hits 의 입력 순서)와 BM25 어휘 매칭 순위를 Reciprocal Rank
// Fusion(RRF, k=60)으로 융합해 재정렬하고 top_k 개로 자른다. hits 는 이미
// 벡터 점수 내림차순으로 정렬되어 있다고 가정한다.
inline auto rerank_lexical(const std::vector<SearchHit> &hits, const std::string &query,
                           int top_k) -> std::vector<SearchHit> {
    if (hits.empty()) {
        return hits;
    }

    const auto query_tokens = tokenize(query);

    std::vector<std::vector<std::string>> doc_tokens;
    doc_tokens.reserve(hits.size());
    for (const auto &hit : hits) {
        doc_tokens.push_back(tokenize(hit.text));
    }
    const auto lexical_scores = bm25_scores(query_tokens, doc_tokens);

    std::vector<std::size_t> lexical_order(hits.size());
    for (std::size_t i = 0; i < hits.size(); ++i) {
        lexical_order[i] = i;
    }
    std::stable_sort(lexical_order.begin(), lexical_order.end(), [&](std::size_t a, std::size_t b) {
        return lexical_scores[a] > lexical_scores[b];
    });
    std::vector<std::size_t> lexical_rank(hits.size());
    for (std::size_t rank = 0; rank < lexical_order.size(); ++rank) {
        lexical_rank[lexical_order[rank]] = rank;
    }

    constexpr double k_rrf = 60.0;
    std::vector<double> fused(hits.size());
    for (std::size_t i = 0; i < hits.size(); ++i) {
        fused[i] = 1.0 / (k_rrf + static_cast<double>(i)) +
                  1.0 / (k_rrf + static_cast<double>(lexical_rank[i]));
    }

    std::vector<std::size_t> final_order(hits.size());
    for (std::size_t i = 0; i < hits.size(); ++i) {
        final_order[i] = i;
    }
    std::stable_sort(final_order.begin(), final_order.end(),
                     [&](std::size_t a, std::size_t b) { return fused[a] > fused[b]; });

    const std::size_t limit =
        top_k > 0 ? (std::min)(static_cast<std::size_t>(top_k), final_order.size())
                  : final_order.size();
    std::vector<SearchHit> result;
    result.reserve(limit);
    for (std::size_t i = 0; i < limit; ++i) {
        result.push_back(hits[final_order[i]]);
    }
    return result;
}

} // namespace ragcli::rag
