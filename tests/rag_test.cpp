#include <gtest/gtest.h>

#include "cmd/registry.hpp"
#include "test_helper.hpp"
#include "wcppcli/wcli.hpp"

using ragcli::test::CoutCapture;
using ragcli::test::run_cli;

TEST(RagCommand, HasExpectedFlags) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    ASSERT_GE(root.subcommands.size(), 3U);
    const auto &rag = *root.subcommands[2];
    EXPECT_EQ(rag.name, "rag");

    bool found_query = false;
    bool found_add = false;
    bool found_title = false;
    bool found_file = false;

    for (const auto &flag : rag.flags) {
        if (flag.name == "query" && flag.shorthand == 'q') {
            found_query = true;
        }
        if (flag.name == "add" && flag.shorthand == 'a') {
            found_add = true;
        }
        if (flag.name == "title") {
            found_title = true;
        }
        if (flag.name == "file" && flag.shorthand == 'f') {
            found_file = true;
        }
    }

    EXPECT_TRUE(found_query);
    EXPECT_TRUE(found_add);
    EXPECT_TRUE(found_title);
    EXPECT_TRUE(found_file);
}

TEST(RagCommand, MissingQueryOrAddReturnsError) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    CoutCapture cap;
    const int exit_code = run_cli(root, {"rag"});
    EXPECT_NE(exit_code, 0);
}
