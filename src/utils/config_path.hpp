#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <wcppcli/wconf.hpp>
#include <wcppcli/wlog.hpp>

namespace ragcli::utils {

// 플랫폼별 설정 디렉토리 경로를 반환한다.
// - Linux:   $XDG_CONFIG_HOME/ragcli  (기본: ~/.config/ragcli)
// - macOS:   ~/Library/Application Support/ragcli
// - Windows: %APPDATA%\ragcli
inline auto get_config_dir() -> std::string {
#if defined(_WIN32)
    const char *appdata = std::getenv("APPDATA");
    if (appdata != nullptr) {
        return std::string(appdata) + "\\ragcli";
    }
    const char *userprofile = std::getenv("USERPROFILE");
    if (userprofile != nullptr) {
        return std::string(userprofile) + "\\AppData\\Roaming\\ragcli";
    }
    return ".";
#elif defined(__APPLE__)
    const char *home = std::getenv("HOME");
    if (home != nullptr) {
        return std::string(home) + "/Library/Application Support/ragcli";
    }
    return ".";
#else
    // Linux / 기타 Unix: XDG Base Directory Specification
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg != nullptr && xdg[0] != '\0') {
        return std::string(xdg) + "/ragcli";
    }
    const char *home = std::getenv("HOME");
    if (home != nullptr) {
        return std::string(home) + "/.config/ragcli";
    }
    return ".";
#endif
}

// 우선순위에 따른 설정 파일 후보 목록을 반환한다 (CWD 우선 -> 전역 순).
inline auto get_config_candidates() -> std::vector<std::string> {
    const std::string config_dir = get_config_dir();
    return {
        "config.yaml",
        ".env",
        config_dir + "/config.yaml",
        config_dir + "/.env",
    };
}

// 활성화된(우선순위가 가장 높은) 설정 파일 경로를 반환한다.
inline auto find_config_path() -> std::string {
    for (const auto &path : get_config_candidates()) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return "";
}

// 계층적 설정 로드:
// 1. 전역 설정 파일이 있으면 먼저 로드 (전역 기본값)
// 2. CWD 설정 파일이 있으면 이어서 로드하여 덮어씀 (CWD 우선 오버라이드)
inline void load_config(wcppcli::WConf &conf) {
    const std::string config_dir = get_config_dir();

    // 1) 전역 설정 로드 (config.yaml -> .env)
    std::string global_path;
    if (std::filesystem::exists(config_dir + "/config.yaml")) {
        global_path = config_dir + "/config.yaml";
    } else if (std::filesystem::exists(config_dir + "/.env")) {
        global_path = config_dir + "/.env";
    }
    if (!global_path.empty()) {
        conf.read_file(global_path);
        wcppcli::WLog::debug("Loaded global config: " + global_path);
    }

    // 2) 현재 디렉토리(CWD) 설정 로드 (config.yaml -> .env) - 전역 설정을 덮어씀
    std::string local_path;
    if (std::filesystem::exists("config.yaml")) {
        local_path = "config.yaml";
    } else if (std::filesystem::exists(".env")) {
        local_path = ".env";
    }

    // 전역 파일과 CWD 파일이 다른 파일인 경우에만 덮어쓰기 로드
    if (!local_path.empty()) {
        std::error_code ec;
        if (global_path.empty() || !std::filesystem::equivalent(local_path, global_path, ec)) {
            conf.read_file(local_path);
            wcppcli::WLog::debug("Loaded local config (override): " + local_path);
        }
    }
}

// 플랫폼 설정 디렉토리의 기본 설정 파일 경로.
inline auto get_default_config_path() -> std::string {
    return get_config_dir() + "/config.yaml";
}

} // namespace ragcli::utils
