#include <gtest/gtest.h>

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
    }

    EXPECT_TRUE(found_file);
    EXPECT_TRUE(found_model);
    EXPECT_TRUE(found_url);
    EXPECT_TRUE(found_provider);
    EXPECT_TRUE(found_language);
}

TEST(PdfSummarizeCommand, MissingFileReturnsError) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    ragcli::test::CoutCapture cap;
    const int exit_code = ragcli::test::run_cli(root, {"pdf"});
    EXPECT_NE(exit_code, 0);
}