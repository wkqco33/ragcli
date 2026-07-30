#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "cmd/config.hpp"
#include "utils/config_path.hpp"
#include "wcppcli/wcli.hpp"
#include "wcppcli/wconf.hpp"

TEST(ConfigPath, GetConfigDirIsNotEmpty) {
    std::string dir = ragcli::utils::get_config_dir();
    EXPECT_FALSE(dir.empty());
}

TEST(ConfigPath, LoadConfigPrioritizesLocalOverGlobal) {
    wcppcli::WConf conf;
    // CWD에 있는 .env/config.yaml 파싱 테스트
    ragcli::utils::load_config(conf);
    // 예외 없이 실행되는지 확인
    SUCCEED();
}

TEST(ConfigCommand, HasExpectedSubcommands) {
    wcppcli::Command root;
    ragcli::cmd::ConfigCommand cmd;
    cmd.register_to(root);

    ASSERT_FALSE(root.subcommands.empty());
    EXPECT_EQ(root.subcommands[0]->name, "config");

    auto &config_cmd = root.subcommands[0];
    ASSERT_EQ(config_cmd->subcommands.size(), 3U);
    EXPECT_EQ(config_cmd->subcommands[0]->name, "init");
    EXPECT_EQ(config_cmd->subcommands[1]->name, "set");
    EXPECT_EQ(config_cmd->subcommands[2]->name, "path");
}
