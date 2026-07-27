#include <gtest/gtest.h>

#include "cmd/registry.hpp"
#include "test_helper.hpp"
#include "wcppcli/wcli.hpp"

TEST(ChatCommand, HasExpectedFlags) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    ASSERT_GE(root.subcommands.size(), 4U);
    const auto &chat = *root.subcommands[0];
    EXPECT_EQ(chat.name, "chat");

    bool found_model = false;
    bool found_url = false;
    bool found_provider = false;
    for (const auto &flag : chat.flags) {
        if (flag.name == "model" && flag.shorthand == 'm') {
            found_model = true;
        }
        if (flag.name == "url" && flag.shorthand == 'u') {
            found_url = true;
        }
        if (flag.name == "provider") {
            found_provider = true;
        }
    }
    EXPECT_TRUE(found_model);
    EXPECT_TRUE(found_url);
    EXPECT_TRUE(found_provider);
}
