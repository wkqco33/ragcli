#include <gtest/gtest.h>

#include "cmd/registry.hpp"
#include "test_helper.hpp"
#include "wcppcli/wcli.hpp"

TEST(RegisterCommands, AddsSubcommands) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    ASSERT_EQ(root.subcommands.size(), 3U);
    EXPECT_EQ(root.subcommands[0]->name, "greet");
    EXPECT_FALSE(root.subcommands[0]->description.empty());
    EXPECT_EQ(root.subcommands[1]->name, "chat");
    EXPECT_FALSE(root.subcommands[1]->description.empty());
    EXPECT_EQ(root.subcommands[2]->name, "rag");
    EXPECT_FALSE(root.subcommands[2]->description.empty());
}
