#include <gtest/gtest.h>

#include "cmd/registry.hpp"
#include "test_helper.hpp"
#include "wcppcli/wcli.hpp"

TEST(OcrCommand, HasExpectedFlags) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    ASSERT_GE(root.subcommands.size(), 5U);
    const auto &ocr = *root.subcommands[4];
    EXPECT_EQ(ocr.name, "ocr");
    EXPECT_FALSE(ocr.description.empty());

    bool found_file = false;
    bool found_model = false;
    bool found_url = false;
    bool found_provider = false;
    bool found_language = false;

    for (const auto &flag : ocr.flags) {
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

TEST(OcrCommand, MissingFileReturnsError) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    ragcli::test::CoutCapture cap;
    const int exit_code = ragcli::test::run_cli(root, {"ocr"});
    EXPECT_NE(exit_code, 0);
}

TEST(OcrCommand, NonExistentFileReturnsError) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    ragcli::test::CoutCapture cap;
    const int exit_code =
        ragcli::test::run_cli(root, {"ocr", "-f", "/tmp/ragcli_nonexistent_image.png"});
    EXPECT_NE(exit_code, 0);
}