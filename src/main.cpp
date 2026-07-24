#include <exception>
#include <string>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wlog.hpp>

#include "cmd/registry.hpp"

auto main(int argc, char **argv) -> int {
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
