#pragma once

#include <wcppcli/wcli.hpp>

namespace ragcli::cmd {

// 모든 서브커맨드의 베이스. register_to() 에서 wcppcli::Command 를 만들어 루트에 붙인다.
class CommandBase {
  public:
    virtual ~CommandBase() = default;
    virtual void register_to(wcppcli::Command &root) = 0;
};

} // namespace ragcli::cmd
