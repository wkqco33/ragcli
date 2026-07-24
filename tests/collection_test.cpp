#include <gtest/gtest.h>

#include "cmd/registry.hpp"
#include "test_helper.hpp"
#include "wcppcli/wcli.hpp"

using ragcli::test::run_cli;

TEST(CollectionCommand, HasExpectedFlags) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    ASSERT_GE(root.subcommands.size(), 4U);
    const auto &collection = *root.subcommands[3];
    EXPECT_EQ(collection.name, "collection");

    bool found_list = false;
    bool found_info = false;
    bool found_delete = false;
    bool found_collection = false;

    for (const auto &flag : collection.flags) {
        if (flag.name == "list" && flag.shorthand == 'l') {
            found_list = true;
        }
        if (flag.name == "info" && flag.shorthand == 'i') {
            found_info = true;
        }
        if (flag.name == "delete" && flag.shorthand == 'd') {
            found_delete = true;
        }
        if (flag.name == "collection" && flag.shorthand == 'c') {
            found_collection = true;
        }
    }

    EXPECT_TRUE(found_list);
    EXPECT_TRUE(found_info);
    EXPECT_TRUE(found_delete);
    EXPECT_TRUE(found_collection);
}

TEST(CollectionCommand, NoActionFlagReturnsError) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    // list/info/delete 중 아무 것도 지정하지 않으면 네트워크 호출 없이 바로
    // 에러를 반환해야 한다.
    const int exit_code = run_cli(root, {"collection"});
    EXPECT_NE(exit_code, 0);
}
