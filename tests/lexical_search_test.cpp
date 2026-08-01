#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "rag/lexical_search.hpp"
#include "rag/qdrant_port.hpp"

using ragcli::rag::SearchHit;

TEST(LexicalSearch, TokenizesAsciiWordsCaseInsensitively) {
    auto tokens = ragcli::rag::tokenize("Hello World");
    ASSERT_EQ(tokens.size(), 2U);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
}

TEST(LexicalSearch, TokenizesHangulAsCharacterBigrams) {
    auto tokens = ragcli::rag::tokenize("안녕하세요"); // 5음절, 공백 없음
    ASSERT_EQ(tokens.size(), 4U);
    EXPECT_EQ(tokens[0], "안녕");
    EXPECT_EQ(tokens[1], "녕하");
    EXPECT_EQ(tokens[2], "하세");
    EXPECT_EQ(tokens[3], "세요");
}

TEST(LexicalSearch, TokenizesSingleHangulSyllableAsUnigram) {
    auto tokens = ragcli::rag::tokenize("안");
    ASSERT_EQ(tokens.size(), 1U);
    EXPECT_EQ(tokens[0], "안");
}

TEST(LexicalSearch, Bm25ScoresExactMatchHigherThanNoMatch) {
    auto query_tokens = ragcli::rag::tokenize("hello");
    std::vector<std::vector<std::string>> doc_tokens = {
        ragcli::rag::tokenize("hello world this contains the keyword"),
        ragcli::rag::tokenize("completely unrelated text about something else"),
    };
    auto scores = ragcli::rag::bm25_scores(query_tokens, doc_tokens);

    ASSERT_EQ(scores.size(), 2U);
    EXPECT_GT(scores[0], scores[1]);
    EXPECT_DOUBLE_EQ(scores[1], 0.0);
}

TEST(LexicalSearch, RerankMovesExactKeywordMatchAheadOfUnrelatedMiddleRankedHit) {
    SearchHit hit0;
    hit0.text = "다른 내용 전혀 다른 이야기 여기 있음 그냥 아무 말이나";
    hit0.score = 0.9;

    SearchHit hit1;
    hit1.text = "완전 상관없는 두번째 문서 내용도 마찬가지로 딴소리만 가득한 텍스트";
    hit1.score = 0.8;

    SearchHit hit2;
    hit2.text = "안녕하세요 반갑습니다"; // "안녕" 정확히 포함
    hit2.score = 0.7;

    auto result = ragcli::rag::rerank_lexical({hit0, hit1, hit2}, "안녕", /*top_k=*/3);

    ASSERT_EQ(result.size(), 3U);
    EXPECT_EQ(result[0].text, hit0.text); // 벡터 검색 1위는 유지 (RRF 는 두 신호의 합의를 반영)
    EXPECT_EQ(result[1].text, hit2.text); // 키워드 매치 문서가 2위로 상승
    EXPECT_EQ(result[2].text, hit1.text); // 매치 없는 문서가 3위로 밀림
}

TEST(LexicalSearch, RerankLexicalTruncatesToTopK) {
    SearchHit hit0;
    hit0.text = "첫번째 문서";
    SearchHit hit1;
    hit1.text = "두번째 문서";
    SearchHit hit2;
    hit2.text = "세번째 문서";

    auto result = ragcli::rag::rerank_lexical({hit0, hit1, hit2}, "문서", /*top_k=*/2);
    EXPECT_EQ(result.size(), 2U);
}

TEST(LexicalSearch, RerankLexicalHandlesEmptyHits) {
    auto result = ragcli::rag::rerank_lexical({}, "질문", /*top_k=*/5);
    EXPECT_TRUE(result.empty());
}
