#include <gtest/gtest.h>

#include "cmd/pdf_summarize.hpp"
#include "cmd/registry.hpp"
#include "test_helper.hpp"
#include "wcppcli/wcli.hpp"

TEST(PdfSummarizeCommand, HasExpectedFlags) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    ASSERT_GE(root.subcommands.size(), 4U);
    const auto &pdf = *root.subcommands[3];
    EXPECT_EQ(pdf.name, "pdf");
    EXPECT_FALSE(pdf.description.empty());

    bool found_file = false;
    bool found_model = false;
    bool found_url = false;
    bool found_provider = false;
    bool found_language = false;
    bool found_pages = false;
    bool found_max_chars = false;
    bool found_chunk_size = false;
    bool found_map_reduce = false;

    for (const auto &flag : pdf.flags) {
        if (flag.name == "file" && flag.shorthand == 'f') {
            found_file = true;
        }
        if (flag.name == "model" && flag.shorthand == 'm') {
            found_model = true;
        }
        if (flag.name == "url" && flag.shorthand == 'u') {
            found_url = true;
        }
        if (flag.name == "provider") {
            found_provider = true;
        }
        if (flag.name == "language" && flag.shorthand == 'l') {
            found_language = true;
        }
        if (flag.name == "pages" && flag.shorthand == 'p') {
            found_pages = true;
        }
        if (flag.name == "max-chars") {
            found_max_chars = true;
        }
        if (flag.name == "chunk-size") {
            found_chunk_size = true;
        }
        if (flag.name == "map-reduce") {
            found_map_reduce = true;
        }
    }

    EXPECT_TRUE(found_file);
    EXPECT_TRUE(found_model);
    EXPECT_TRUE(found_url);
    EXPECT_TRUE(found_provider);
    EXPECT_TRUE(found_language);
    EXPECT_TRUE(found_pages);
    EXPECT_TRUE(found_max_chars);
    EXPECT_TRUE(found_chunk_size);
    EXPECT_TRUE(found_map_reduce);
}

TEST(PdfSummarizeCommand, MissingFileReturnsError) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    ragcli::test::CoutCapture cap;
    const int exit_code = ragcli::test::run_cli(root, {"pdf"});
    EXPECT_NE(exit_code, 0);
}

TEST(PdfSummarizeCommand, ParsePageRanges) {
    auto ranges = ragcli::cmd::PdfSummarizeCommand::parse_page_ranges("1-10,15,20-25");
    ASSERT_EQ(ranges.size(), 3U);
    EXPECT_EQ(ranges[0].first, 1);
    EXPECT_EQ(ranges[0].second, 10);
    EXPECT_EQ(ranges[1].first, 15);
    EXPECT_EQ(ranges[1].second, 15);
    EXPECT_EQ(ranges[2].first, 20);
    EXPECT_EQ(ranges[2].second, 25);
}

TEST(PdfSummarizeCommand, SplitTextForMapReduce) {
    std::string text;
    for (int i = 0; i < 100; ++i) {
        text += "Paragraph " + std::to_string(i) + " contains some sample content.\n\n";
    }

    auto chunks = ragcli::cmd::PdfSummarizeCommand::split_text_for_map_reduce(text, 500);
    EXPECT_GT(chunks.size(), 1U);
    
    std::string reassembled;
    for (const auto &c : chunks) {
        reassembled += c;
    }
    EXPECT_EQ(reassembled, text);
}