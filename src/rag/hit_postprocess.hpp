#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "rag/qdrant_port.hpp"

namespace ragcli::rag {

namespace {
// (source, chunk_index) 를 std::unordered_set 의 키로 쓰기 위한 해셔.
struct SourceChunkHash {
    std::size_t operator()(const std::pair<std::string, int> &k) const noexcept {
        std::size_t h = std::hash<std::string>{}(k.first);
        h ^= static_cast<std::size_t>(k.second) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

} // namespace

// 각 hit 의 chunk_index ± radius 이웃 청크를 같은 source 안에서 후보에 추가한다.
// radius <= 0 이면 no-op. 추가된 이웃은 score 0.0 으로 원 순위를 유지한다.
inline auto expand_neighbors(const std::vector<SearchHit> &hits, const QdrantPort &port,
                             int radius) -> std::vector<SearchHit> {
    if (radius <= 0) {
        return hits;
    }

    std::vector<SearchHit> expanded = hits;

    std::unordered_set<std::pair<std::string, int>, SourceChunkHash> seen;
    seen.reserve(hits.size());
    for (const auto &hit : hits) {
        if (!hit.source.empty()) {
            seen.emplace(hit.source, hit.chunk_index);
        }
    }

    for (const auto &hit : hits) {
        if (hit.source.empty() || hit.is_image) {
            continue; // 출처를 모르거나 이미지 청크는 확장 대상에서 제외
        }
        const int min_index = (std::max)(hit.chunk_index - radius, 0);
        const int max_index = hit.chunk_index + radius;

        auto neighbors = port.fetch_neighbors(hit.source, min_index, max_index);
        for (auto &neighbor : neighbors) {
            if (!seen.emplace(neighbor.source, neighbor.chunk_index).second) {
                continue;
            }
            neighbor.score = 0.0;
            expanded.push_back(std::move(neighbor));
        }
    }

    return expanded;
}

// a 의 접미사와 b 의 접두사가 겹치는 최장 길이를 바이트 단위로 찾는다.
// 비교 범위는 max_check 로 제한해 큰 청크에서도 비용을 억제한다.
inline auto find_overlap_len(const std::string &a, const std::string &b) -> std::size_t {
    const std::size_t max_check = (std::min)({a.size(), b.size(), static_cast<std::size_t>(4000)});
    for (std::size_t len = max_check; len > 0; --len) {
        if (a.compare(a.size() - len, len, b, 0, len) == 0) {
            return len;
        }
    }
    return 0;
}

// next 를 target 뒤에 이어붙이되, 겹치는 구간은 한 번만 남긴다.
inline void merge_hit_text(SearchHit &target, const SearchHit &next) {
    const std::size_t overlap = find_overlap_len(target.text, next.text);
    target.text += next.text.substr(overlap);
    target.score = (std::max)(target.score, next.score);
    if (target.chunk_total == 0) {
        target.chunk_total = next.chunk_total;
    }
}

// 같은 source 안에서 chunk_index 가 연속인 히트들을 하나로 병합해 중복 텍스트를
// 제거하고 조각을 줄인다. 병합 결과는 score 내림차순으로 재정렬해 원래의
// 검색 순위 감각을 유지한다 (병합된 히트의 score 는 구성원 중 최댓값).
inline auto merge_adjacent(std::vector<SearchHit> hits) -> std::vector<SearchHit> {
    if (hits.size() <= 1) {
        return hits;
    }

    std::stable_sort(hits.begin(), hits.end(), [](const SearchHit &a, const SearchHit &b) {
        if (a.source != b.source) {
            return a.source < b.source;
        }
        return a.chunk_index < b.chunk_index;
    });

    std::vector<SearchHit> merged;
    std::vector<int> last_index; // merged 의 각 원소가 흡수한 마지막 chunk_index
    merged.reserve(hits.size());
    last_index.reserve(hits.size());

    for (auto &hit : hits) {
        if (!merged.empty() && !merged.back().source.empty() &&
            merged.back().source == hit.source && last_index.back() + 1 == hit.chunk_index) {
            merge_hit_text(merged.back(), hit);
            last_index.back() = hit.chunk_index;
            continue;
        }
        last_index.push_back(hit.chunk_index);
        merged.push_back(std::move(hit));
    }

    std::stable_sort(merged.begin(), merged.end(),
                     [](const SearchHit &a, const SearchHit &b) { return a.score > b.score; });
    return merged;
}

} // namespace ragcli::rag
