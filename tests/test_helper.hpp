#ifndef RAGCLI_TESTS_TEST_HELPER_HPP_
#define RAGCLI_TESTS_TEST_HELPER_HPP_

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "wcppcli/wcli.hpp"

namespace ragcli::test {

// wcppcli::Command::execute(int, char**) 에 맞춰 argc/argv 를 흉내낸다.
// 첫 인자는 관례상 프로그램 이름.
inline auto run_cli(wcppcli::Command &root, const std::vector<std::string> &args) -> int {
    std::vector<char *> argv;
    argv.reserve(args.size() + 1);
    argv.push_back(const_cast<char *>("ragcli"));
    for (const auto &arg : args) {
        argv.push_back(const_cast<char *>(arg.c_str()));
    }
    return root.execute(static_cast<int>(argv.size()), argv.data());
}

// std::cout 을 잠시 가로채 문자열로 모은다. RAII 로 원복.
class CoutCapture {
  public:
    CoutCapture() : old_(std::cout.rdbuf(buf_.rdbuf())) {}
    ~CoutCapture() {
        std::cout.rdbuf(old_);
    }

    auto str() const -> std::string {
        return buf_.str();
    }

  private:
    std::ostringstream buf_;
    std::streambuf *old_;
};

} // namespace ragcli::test

#endif // RAGCLI_TESTS_TEST_HELPER_HPP_
