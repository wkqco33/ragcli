#pragma once

#include <string>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wlog.hpp>

#include "command.hpp"

namespace ragcli::cmd {

// `ragcli greet --name <NAME>` 을 처리하는 서브커맨드.
// 이름을 받아 인사말을 출력한다.
class GreetCommand : public CommandBase {
  public:
    void register_to(wcppcli::Command &root) override {
        auto cmd = std::make_unique<wcppcli::Command>();
        cmd->name = "greet";
        cmd->description = "Print a greeting message";

        wcppcli::Flag name_flag;
        name_flag.name = "name";
        name_flag.shorthand = 'n';
        name_flag.description = "Name to greet";
        name_flag.value_ptr = &name_; // this 가 살아있는 동안 안전
        cmd->add_flag(name_flag);

        cmd->handler = [this](const wcppcli::Command &) {
            wcppcli::WLog::success("Hello, " + name_ + "! (from ragcli)");
            return 0;
        };

        root.add_command(std::move(cmd));
    }

  private:
    std::string name_ = "ragcli";
};

} // namespace ragcli::cmd
