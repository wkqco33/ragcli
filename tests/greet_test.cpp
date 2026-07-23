#include <gtest/gtest.h>

#include "cmd/registry.hpp"
#include "test_helper.hpp"
#include "wcppcli/wcli.hpp"

using ragcli::test::CoutCapture;
using ragcli::test::run_cli;

TEST(GreetCommand, HasNameFlag) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    ASSERT_FALSE(root.subcommands.empty());
    const auto &greet = *root.subcommands[0];

    ASSERT_EQ(greet.flags.size(), 1U);
    EXPECT_EQ(greet.flags[0].name, "name");
    EXPECT_EQ(greet.flags[0].shorthand, 'n');
}

TEST(GreetCommand, LongFlagReturnsZeroAndPrintsGreeting) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    CoutCapture cap;
    const int exit_code = run_cli(root, {"greet", "--name", "World"});
    const auto out = cap.str();

    EXPECT_EQ(exit_code, 0);
    EXPECT_NE(out.find("Hello, World!"), std::string::npos);
    EXPECT_NE(out.find("from ragcli"), std::string::npos);
}

TEST(GreetCommand, ShortFlagUsesShorthand) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    CoutCapture cap;
    const int exit_code = run_cli(root, {"greet", "-n", "CLI"});
    const auto out = cap.str();

    EXPECT_EQ(exit_code, 0);
    EXPECT_NE(out.find("Hello, CLI!"), std::string::npos);
}

TEST(GreetCommand, DefaultsToProjectName) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    CoutCapture cap;
    const int exit_code = run_cli(root, {"greet"});
    const auto out = cap.str();

    EXPECT_EQ(exit_code, 0);
    EXPECT_NE(out.find("Hello, ragcli!"), std::string::npos);
}

TEST(GreetCommand, UnknownFlagReturnsNonZero) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    CoutCapture cap;
    const int exit_code = run_cli(root, {"greet", "--bogus"});
    EXPECT_NE(exit_code, 0);
}

TEST(GreetCommand, EqualsSyntaxParsesValue) {
    wcppcli::Command root;
    auto holders = ragcli::cmd::register_commands(root);

    CoutCapture cap;
    const int exit_code = run_cli(root, {"greet", "--name=Equals"});
    const auto out = cap.str();

    EXPECT_EQ(exit_code, 0);
    EXPECT_NE(out.find("Hello, Equals!"), std::string::npos);
}
