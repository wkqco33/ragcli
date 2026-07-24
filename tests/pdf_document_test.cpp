#include <string>

#include <gtest/gtest.h>

#include "document/pdf_document.hpp"

TEST(PdfFileSource, ExtractsTextFromSamplePdf) {
    const std::string path = std::string(RAGCLI_TEST_FIXTURES_DIR) + "/sample.pdf";

    ragcli::document::PdfFileSource source(path);
    auto pages = source.extract();

    ASSERT_FALSE(pages.empty());

    bool found_expected_text = false;
    for (const auto &page : pages) {
        if (page.text.find("Hello, cpppdf!") != std::string::npos) {
            found_expected_text = true;
            EXPECT_FALSE(page.is_image);
            EXPECT_EQ(page.page_index, 1);
        }
    }
    EXPECT_TRUE(found_expected_text);
}

TEST(PdfFileSource, ThrowsOnMissingFile) {
    ragcli::document::PdfFileSource source("/tmp/ragcli_nonexistent_sample.pdf");
    EXPECT_THROW(source.extract(), std::runtime_error);
}
