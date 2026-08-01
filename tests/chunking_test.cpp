#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "chunking/auto_chunker.hpp"
#include "chunking/markdown_chunker.hpp"
#include "chunking/simple_chunker.hpp"
#include "document/document_source.hpp"
#include "utils/utf8.hpp"

using ragcli::chunking::AutoChunker;
using ragcli::chunking::ChunkSize;
using ragcli::chunking::MarkdownChunker;
using ragcli::chunking::Overlap;
using ragcli::chunking::SimpleChunker;
using ragcli::document::ExtractedPage;

namespace {

// 표준 UTF-8 디코딩 규칙에 따라 텍스트가 완전한 코드포인트들로만 구성되는지 검증한다
// (중간에 잘린 멀티바이트 시퀀스가 있으면 false).
auto is_valid_utf8(const std::string &text) -> bool {
    std::size_t i = 0;
    while (i < text.size()) {
        const auto c = static_cast<unsigned char>(text[i]);
        std::size_t len = 0;
        if ((c & 0x80) == 0x00) {
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        } else {
            return false;
        }
        if (i + len > text.size()) {
            return false;
        }
        for (std::size_t k = 1; k < len; ++k) {
            if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) {
                return false;
            }
        }
        i += len;
    }
    return true;
}

auto build_tagged_text(int count) -> std::string {
    std::string result;
    for (int i = 0; i < count; ++i) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%04d", i);
        result += buf;
    }
    return result;
}

auto single_page(std::string text, std::string title) -> std::vector<ExtractedPage> {
    ExtractedPage page;
    page.text = std::move(text);
    page.title = std::move(title);
    return {std::move(page)};
}

} // namespace

TEST(SimpleChunker, DoesNotSplitMultibyteUtf8CharactersWithoutWhitespace) {
    std::string text;
    for (int i = 0; i < 300; ++i) {
        text += "\xea\xb0\x80"; // '가', 공백 없이 반복 -> 폴백 경로(snap_back) 강제
    }

    SimpleChunker chunker(ChunkSize{80}, Overlap{10});
    auto chunks = chunker.chunk(single_page(text, "korean_no_space.txt"), "korean_no_space.txt");

    ASSERT_GT(chunks.size(), 1U);
    for (const auto &chunk : chunks) {
        EXPECT_TRUE(is_valid_utf8(chunk.text));
        EXPECT_LE(ragcli::utils::utf8::char_count(chunk.text, chunk.text.size()), 80U);
    }
}

TEST(SimpleChunker, PrefersParagraphBoundaryOverMidText) {
    const std::string para1(80, 'A');
    const std::string para2(80, 'B');
    const std::string text = para1 + "\n\n" + para2;

    SimpleChunker chunker(ChunkSize{100}, Overlap{10});
    auto chunks = chunker.chunk(single_page(text, "para.txt"), "para.txt");

    ASSERT_GE(chunks.size(), 2U);
    EXPECT_EQ(chunks[0].text.find('B'), std::string::npos);
}

TEST(SimpleChunker, OverlapRegionMatchesBetweenAdjacentChunks) {
    const std::string text = build_tagged_text(150); // 600자, 공백/구두점 없음 -> 정확한 산술 경계
    SimpleChunker chunker(ChunkSize{100}, Overlap{20});
    auto chunks = chunker.chunk(single_page(text, "tags.txt"), "tags.txt");

    ASSERT_GT(chunks.size(), 2U);
    for (std::size_t i = 0; i + 2 < chunks.size(); ++i) { // 마지막 청크는 병합으로 길이가 다를 수 있어 제외
        ASSERT_GE(chunks[i].text.size(), 20U);
        const std::string tail = chunks[i].text.substr(chunks[i].text.size() - 20);
        const std::string head = chunks[i + 1].text.substr(0, 20);
        EXPECT_EQ(tail, head) << "chunk " << i << " vs " << (i + 1);
    }
}

TEST(SimpleChunker, MergesTinyTailChunkIntoPrevious) {
    // chunk_size=100, min_tail=25자. 총 105자 -> 마지막 조각이 5자 미만으로 남으면 흡수되어야 함.
    const std::string text(105, 'x');
    SimpleChunker chunker(ChunkSize{100}, Overlap{0});
    auto chunks = chunker.chunk(single_page(text, "tail.txt"), "tail.txt");

    ASSERT_EQ(chunks.size(), 1U);
    EXPECT_EQ(chunks[0].text.size(), 105U);
}

TEST(MarkdownChunker, BuildsHeadingPathAcrossLevels) {
    const std::string filler(150, 'x'); // min_chars(128) 를 넘겨 병합되지 않도록 함
    const std::string md = "# Top\n" + filler + "\n## Mid\n" + filler + "\n### Leaf\n" + filler + "\n";

    MarkdownChunker chunker;
    auto chunks = chunker.chunk(single_page(md, "doc.md"), "doc.md");

    ASSERT_EQ(chunks.size(), 3U);
    EXPECT_EQ(chunks[0].title, "Top");
    EXPECT_EQ(chunks[0].heading_path, "Top");
    EXPECT_EQ(chunks[1].title, "Mid");
    EXPECT_EQ(chunks[1].heading_path, "Top > Mid");
    EXPECT_EQ(chunks[2].title, "Leaf");
    EXPECT_EQ(chunks[2].heading_path, "Top > Mid > Leaf");
}

TEST(MarkdownChunker, IgnoresHeadingLikeLinesInsideFencedCodeBlocks) {
    const std::string filler(150, 'x');
    const std::string md = "# Real Heading\n" + filler +
                           "\n```\n# not a heading\n```\n" + std::string(150, 'y') + "\n";

    MarkdownChunker chunker;
    auto chunks = chunker.chunk(single_page(md, "doc.md"), "doc.md");

    ASSERT_EQ(chunks.size(), 1U); // 실제 헤딩은 하나뿐이므로 섹션도 하나
    EXPECT_EQ(chunks[0].title, "Real Heading");
    EXPECT_NE(chunks[0].text.find("# not a heading"), std::string::npos);
}

TEST(MarkdownChunker, SplitsOversizedSectionWithFallbackChunkerAndKeepsHeadingPath) {
    const std::string big_section(1000, 'z'); // 기본 chunk_size(512) 초과
    const std::string md = "# Big\n" + big_section + "\n";

    MarkdownChunker chunker; // 기본 chunk_size=512, overlap=64
    auto chunks = chunker.chunk(single_page(md, "doc.md"), "doc.md");

    ASSERT_GT(chunks.size(), 1U);
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        EXPECT_EQ(chunks[i].heading_path, "Big");
        EXPECT_EQ(chunks[i].title, "Big");
        EXPECT_EQ(chunks[i].chunk_index, static_cast<int>(i));
    }
}

TEST(AutoChunker, RoutesPagesByFileExtension) {
    ExtractedPage md_page;
    md_page.text = "# Heading\n" + std::string(150, 'a') + "\n";
    md_page.title = "notes.md";
    md_page.source_path = "docs/notes.md";

    ExtractedPage txt_page;
    txt_page.text = std::string(150, 'b');
    txt_page.title = "notes.txt";
    txt_page.source_path = "docs/notes.txt";

    AutoChunker chunker;
    auto chunks = chunker.chunk({md_page, txt_page}, "docs");

    ASSERT_EQ(chunks.size(), 2U);
    EXPECT_EQ(chunks[0].title, "Heading"); // 마크다운 청커가 헤딩을 title 로 사용
    EXPECT_EQ(chunks[0].source, "docs/notes.md");
    EXPECT_EQ(chunks[0].chunk_index, 0);
    EXPECT_EQ(chunks[1].title, "notes.txt"); // 심플 청커는 page.title 을 그대로 사용
    EXPECT_EQ(chunks[1].source, "docs/notes.txt");
    EXPECT_EQ(chunks[1].chunk_index, 1);
}
