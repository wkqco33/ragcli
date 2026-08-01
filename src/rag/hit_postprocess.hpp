#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "rag/qdrant_port.hpp"

namespace ragcli::rag {

// hits 각각에 대해 같은 source 안의 chunk_index ± radius 이웃 청크를 QdrantPort 로
// 가져와 후보에 추가한다. radius <= 0 이면 아무 것도 하지 않는다. 새로 추가된
// 이웃 청크는 score 를 0.0 으로 낮춰 원래 검색 순위에 영향을 주지 않게 하고,
// merge_adjacent 가 인접한 히트와 하나로 합치도록 한다.
inline auto expand_neighbors(const std::vector<SearchHit> &hits, const QdrantPort &port,
                             int radius) -> std::vector<SearchHit> {
    if (radius <= 0) {
        return hits;
    }

    std::vector<SearchHit> expanded = hits;

    std::vector<std::pair<std::string, int>> seen;
    seen.reserve(hits.size());
    for (const auto &hit : hits) {
        if (!hit.source.empty()) {
            seen.emplace_back(hit.source, hit.chunk_index);
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
            const bool already_present =
                std::any_of(seen.begin(), seen.end(), [&](const std::pair<std::string, int> &key) {
                    return key.first == neighbor.source && key.second == neighbor.chunk_index;
                });
            if (already_present) {
                continue;
            }
            seen.emplace_back(neighbor.source, neighbor.chunk_index);
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
