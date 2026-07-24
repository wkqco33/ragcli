#pragma once

#include <string>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wlog.hpp>

#include "command.hpp"
#include "flag_helper.hpp"

namespace ragcli::cmd {

// `ragcli greet --name <NAME>` 을 처리하는 서브커맨드.
// 이름을 받아 인사말을 출력한다.
class GreetCommand : public CommandBase {
  public:
    void register_to(wcppcli::Command &root) override {
        auto cmd = std::make_unique<wcppcli::Command>();
        cmd->name = "greet";
        cmd->description = "Print a greeting message";

        add_string_flag(*cmd, "name", 'n', "Name to greet", &name_);

        cmd->handler = [this](const wcppcli::Command & /*unused*/) {
            wcppcli::WLog::success("Hello, " + name_ + "! (from ragcli)");
            return 0;
        };

        root.add_command(std::move(cmd));
    }

  private:
    std::string name_ = "ragcli";
};

} // namespace ragcli::cmd
