#pragma once

#include <string>
#include <wcppcli/wcli.hpp>

namespace ragcli::cmd {

// 커맨드에 string 타입 플래그를 등록한다. shorthand 가 0 이면 짧은 옵션을 등록하지 않는다.
inline void add_string_flag(wcppcli::Command &cmd, const std::string &name, char shorthand,
                            const std::string &description, std::string *value_ptr) {
    wcppcli::Flag flag;
    flag.name = name;
    if (shorthand != 0) {
        flag.shorthand = shorthand;
    }
    flag.description = description;
    flag.value_ptr = value_ptr;
    cmd.add_flag(flag);
}

// int 타입 플래그 등록 (string 타입과 동일한 규칙).
inline void add_int_flag(wcppcli::Command &cmd, const std::string &name, char shorthand,
                         const std::string &description, int *value_ptr) {
    wcppcli::Flag flag;
    flag.name = name;
    if (shorthand != 0) {
        flag.shorthand = shorthand;
    }
    flag.description = description;
    flag.value_ptr = value_ptr;
    cmd.add_flag(flag);
}

// bool 타입 플래그 등록 (string 타입과 동일한 규칙).
inline void add_bool_flag(wcppcli::Command &cmd, const std::string &name, char shorthand,
                          const std::string &description, bool *value_ptr) {
    wcppcli::Flag flag;
    flag.name = name;
    if (shorthand != 0) {
        flag.shorthand = shorthand;
    }
    flag.description = description;
    flag.value_ptr = value_ptr;
    cmd.add_flag(flag);
}

} // namespace ragcli::cmd
