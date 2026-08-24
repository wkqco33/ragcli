#include <exception>
#include <string>
#include <vector>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wlog.hpp>

#include "cmd/registry.hpp"

#if defined(_WIN32)
#include <shellapi.h>
#include <windows.h>
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

        auto holders = ragcli::cmd::register_commands(root);

#if defined(_WIN32)
        int wide_argc = 0;
        LPWSTR *wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
        if (wide_argv != nullptr) {
            std::vector<std::string> utf8_args;
            utf8_args.reserve(wide_argc);
            std::vector<char *> utf8_argv;
            utf8_argv.reserve(wide_argc + 1);

            for (int i = 0; i < wide_argc; ++i) {
                int size_needed =
                    WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, nullptr, 0, nullptr, nullptr);
                if (size_needed > 1) {
                    std::string u8str(size_needed - 1, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, u8str.data(), size_needed,
                                        nullptr, nullptr);
                    utf8_args.push_back(std::move(u8str));
                } else {
                    utf8_args.push_back("");
                }
            }
            LocalFree(wide_argv);

            for (auto &s : utf8_args) {
                utf8_argv.push_back(s.data());
            }
            utf8_argv.push_back(nullptr);

            return root.execute(static_cast<int>(utf8_args.size()), utf8_argv.data());
        }
#endif

        return root.execute(argc, argv);
    } catch (const std::exception &e) {
        wcppcli::WLog::error(std::string("Fatal Error: ") + e.what());
        return 1;
    } catch (...) {
        wcppcli::WLog::error("Unknown fatal error occurred.");
        return 1;
    }
}
