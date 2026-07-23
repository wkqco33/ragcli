#pragma once

#include <wcppcli/wcli.hpp>

namespace ragcli::cmd {

// 모든 서브커맨드의 베이스.
// 커맨드 상태는 파생 클래스의 멤버로 들고 있고, register_to() 에서
// wcppcli::Command 를 만들어 루트에 붙인다.
// 인스턴스는 registry 가 std::unique_ptr<CommandBase> 로 소유하며,
// main 이 root.execute() 를 끝낼 때까지 살아있으므로
// value_ptr / 핸들러 캡처가 가리키는 멤버 수명이 보장된다.
class CommandBase {
  public:
    virtual ~CommandBase() = default;
    virtual void register_to(wcppcli::Command &root) = 0;
};

} // namespace ragcli::cmd
