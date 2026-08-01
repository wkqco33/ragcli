#include <exception>
#include <string>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wlog.hpp>

#include "cmd/registry.hpp"

#if defined(_WIN32)
#  include <windows.h>
#endif

namespace {
void configure_console_utf8() {
#if defined(_WIN32)
    // Windows 콘솔의 기본 코드 페이지는 CP949 이므로, UTF-8 문자열을
    // std::cout/std::cerr 로 출력하면 한글이 깨진다. 프로세스 시작 시
    // 입/출력 코드 페이지를 모두 UTF-8(CP 65001) 로 맞춘다.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}
} // namespace

auto main(int argc, char **argv) -> int {
    configure_console_utf8();
    try {
        wcppcli::Command root;
        root.name = "ragcli";
        root.description = "ragcli";

        // 커맨드 상태 홀더는 execute() 동안 살아있어야 하므로
        // main 의 지역 변수로 들고 있는다.
        auto holders = ragcli::cmd::register_commands(root);

        return root.execute(argc, argv);
    } catch (const std::exception &e) {
        wcppcli::WLog::error(std::string("Fatal Error: ") + e.what());
        return 1;
    } catch (...) {
        wcppcli::WLog::error("Unknown fatal error occurred.");
        return 1;
    }
}
