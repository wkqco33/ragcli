#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "mock_qdrant_port.hpp"
#include "rag/hit_postprocess.hpp"
#include "rag/prompt_builder.hpp"
#include "rag/qdrant_port.hpp"

using ragcli::rag::SearchHit;
using ragcli::test::MockQdrantPort;

TEST(PromptBuilder, RendersDocBlocksWithMetadataAndOmitsEmptyAttributes) {
    SearchHit with_meta;
    with_meta.text = "인덱싱은 파일을 청크로 나눈 뒤 임베딩합니다.";
    with_meta.source = "docs/architecture.md";
    with_meta.heading_path = "설계 > 인덱싱";
    with_meta.chunk_index = 1;
    with_meta.chunk_total = 7;
    with_meta.score = 0.8312;

    SearchHit bare;
    bare.text = "부가 설명 텍스트";
    bare.score = 0.5;

    const std::string prompt =
        ragcli::rag::build_query_prompt({with_meta, bare}, "인덱싱은 어떻게 동작하나?");

    EXPECT_NE(prompt.find("<doc id=\"1\" source=\"docs/architecture.md\" section=\"설계 > 인덱싱\" "
                          "chunk=\"2/7\" score=\"0.8312\">"),
              std::string::npos);
    EXPECT_NE(prompt.find("<doc id=\"2\" score=\"0.5000\">"), std::string::npos);
    EXPECT_EQ(prompt.find("source=\"\""), std::string::npos); // 빈 속성은 생략되어야 함
    EXPECT_NE(prompt.find("[1]"), std::string::npos);         // 인용 지시 포함
    EXPECT_NE(prompt.find("질문: 인덱싱은 어떻게 동작하나?"), std::string::npos);
}

TEST(PromptBuilder, MarksImageHitsInsteadOfRawPlaceholderText) {
    SearchHit image_hit;
    image_hit.text = "[Image] chart.png";
    image_hit.is_image = true;
    image_hit.score = 0.7;

    const std::string prompt = ragcli::rag::build_query_prompt({image_hit}, "차트 설명해줘");
    EXPECT_NE(prompt.find("이미지"), std::string::npos);
}

TEST(PromptBuilder, EmptyHitsProducesNoDocumentPlaceholder) {
    const std::string prompt = ragcli::rag::build_query_prompt({}, "질문");
    EXPECT_NE(prompt.find("(문서 없음)"), std::string::npos);
}

TEST(HitPostprocess, MergesAdjacentChunksAndDedupesOverlap) {
    SearchHit a;
    a.text = "안녕하세요 저는 랙클라이입니다 오늘은";
    a.source = "doc.txt";
    a.chunk_index = 0;
    a.score = 0.9;

    SearchHit b;
    b.text = "오늘은 청킹 테스트를 진행합니다";
    b.source = "doc.txt";
    b.chunk_index = 1;
    b.score = 0.7;

    auto merged = ragcli::rag::merge_adjacent({a, b});

    ASSERT_EQ(merged.size(), 1U);
    EXPECT_EQ(merged[0].text, "안녕하세요 저는 랙클라이입니다 오늘은 청킹 테스트를 진행합니다");
    EXPECT_DOUBLE_EQ(merged[0].score, 0.9);
}

TEST(HitPostprocess, DoesNotMergeNonAdjacentChunks) {
    SearchHit a;
    a.text = "A";
    a.source = "doc.txt";
    a.chunk_index = 0;
    a.score = 0.9;

    SearchHit b;
    b.text = "B";
    b.source = "doc.txt";
    b.chunk_index = 5;
    b.score = 0.7;

    auto merged = ragcli::rag::merge_adjacent({a, b});
    EXPECT_EQ(merged.size(), 2U);
}

TEST(HitPostprocess, ExpandNeighborsAddsSurroundingChunksAndMergesForward) {
    SearchHit hit;
    hit.text = "핵심 문단";
    hit.source = "doc.txt";
    hit.chunk_index = 5;
    hit.score = 0.9;

    SearchHit neighbor;
    neighbor.text = "다음 문단";
    neighbor.source = "doc.txt";
    neighbor.chunk_index = 6;
    neighbor.score = 0.0;

    MockQdrantPort port;
    port.neighbor_results_ = {neighbor};

    auto expanded = ragcli::rag::expand_neighbors({hit}, port, /*radius=*/1);
    ASSERT_EQ(expanded.size(), 2U);

    auto merged = ragcli::rag::merge_adjacent(expanded);
    ASSERT_EQ(merged.size(), 1U);
    EXPECT_EQ(merged[0].text, "핵심 문단다음 문단");
    EXPECT_DOUBLE_EQ(merged[0].score, 0.9);
}

TEST(HitPostprocess, ExpandNeighborsIsNoOpWhenRadiusIsZero) {
    SearchHit hit;
    hit.text = "핵심 문단";
    hit.source = "doc.txt";
    hit.chunk_index = 5;
    hit.score = 0.9;

    MockQdrantPort port;
    port.neighbor_results_ = {hit}; // 호출되면 안 됨을 간접 확인

    auto expanded = ragcli::rag::expand_neighbors({hit}, port, /*radius=*/0);
    EXPECT_EQ(expanded.size(), 1U);
}
