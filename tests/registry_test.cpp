#include <gtest/gtest.h>

#include "cmd/registry.hpp"
#include "test_helper.hpp"
#include "wcppcli/wcli.hpp"

TEST(RegisterCommands, AddsSubcommands) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    ASSERT_EQ(root.subcommands.size(), 6U);
    EXPECT_EQ(root.subcommands[0]->name, "chat");
    EXPECT_FALSE(root.subcommands[0]->description.empty());
    EXPECT_EQ(root.subcommands[1]->name, "rag");
    EXPECT_FALSE(root.subcommands[1]->description.empty());
    EXPECT_EQ(root.subcommands[2]->name, "collection");
    EXPECT_FALSE(root.subcommands[2]->description.empty());
    EXPECT_EQ(root.subcommands[3]->name, "pdf");
    EXPECT_FALSE(root.subcommands[3]->description.empty());
    EXPECT_EQ(root.subcommands[4]->name, "ocr");
    EXPECT_FALSE(root.subcommands[4]->description.empty());
    EXPECT_EQ(root.subcommands[5]->name, "config");
    EXPECT_FALSE(root.subcommands[5]->description.empty());
}

TEST(RegisterCommands, SetsApplicationVersion) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    EXPECT_EQ(root.version, "0.1.0");
}
