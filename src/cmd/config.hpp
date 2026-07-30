#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wconf.hpp>
#include <wcppcli/wlog.hpp>

#include "command.hpp"
#include "utils/config_path.hpp"

namespace ragcli::cmd {

// `ragcli config` 서브커맨드.
// 설정 파일의 조회, 초기화, 값 설정을 수행한다.
//
//   ragcli config             현재 설정 내용 출력
//   ragcli config init        플랫폼 config 디렉토리에 config.yaml 생성
//   ragcli config set KEY VAL 설정 파일의 키 값 변경
//   ragcli config path        설정 파일 경로 출력
class ConfigCommand : public CommandBase {
  public:
    void register_to(wcppcli::Command &root) override {
        auto cmd = std::make_unique<wcppcli::Command>();
        cmd->name = "config";
        cmd->description = "Manage ragcli configuration (show, init, set, path)";

        // `ragcli config` (인자 없이) → 현재 설정 출력
        cmd->handler = [](const wcppcli::Command & /*unused*/) { return run_show(); };

        // --- 서브커맨드: init ---
        {
            auto init = std::make_unique<wcppcli::Command>();
            init->name = "init";
            init->description = "Initialize config.yaml in the platform config directory";
            init->handler = [this](const wcppcli::Command & /*unused*/) { return run_init(); };
            cmd->add_command(std::move(init));
        }

        // --- 서브커맨드: set ---
        {
            auto set = std::make_unique<wcppcli::Command>();
            set->name = "set";
            set->description = "Set a config value (usage: ragcli config set KEY VALUE)";
            set->handler = [](const wcppcli::Command &c) { return run_set(c); };
            cmd->add_command(std::move(set));
        }

        // --- 서브커맨드: path ---
        {
            auto path = std::make_unique<wcppcli::Command>();
            path->name = "path";
            path->description = "Print the active config file path";
            path->handler = [](const wcppcli::Command & /*unused*/) { return run_path(); };
            cmd->add_command(std::move(path));
        }

        root.add_command(std::move(cmd));
    }

  private:
    // ragcli config — 현재 설정 내용 출력
    static auto run_show() -> int {
        const std::string path = ragcli::utils::find_config_path();
        if (path.empty()) {
            wcppcli::WLog::warn("No config file found.");
            wcppcli::WLog::info("Run 'ragcli config init' to create one.");
            wcppcli::WLog::info("Search paths:");
            for (const auto &candidate : ragcli::utils::get_config_candidates()) {
                wcppcli::WLog::info("  " + candidate);
            }
            return 0;
        }

        wcppcli::WLog::info("Config file: " + path);
        std::cout << "\n";

        std::ifstream file(path);
        if (!file.is_open()) {
            wcppcli::WLog::error("Failed to read: " + path);
            return 1;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::cout << line << "\n";
        }
        return 0;
    }

    // ragcli config init — 기본 config.yaml 생성
    auto run_init() -> int {
        const std::string config_dir = ragcli::utils::get_config_dir();
        const std::string config_path = ragcli::utils::get_default_config_path();

        if (std::filesystem::exists(config_path)) {
            wcppcli::WLog::warn("Config file already exists: " + config_path);
            wcppcli::WLog::info("Use 'ragcli config set KEY VALUE' to modify values.");
            return 0;
        }

        // 디렉토리가 없으면 생성
        std::filesystem::create_directories(config_dir);

        std::ofstream file(config_path);
        if (!file.is_open()) {
            wcppcli::WLog::error("Failed to create: " + config_path);
            return 1;
        }

        file << generate_default_yaml();
        file.close();

        wcppcli::WLog::success("Config created: " + config_path);
        wcppcli::WLog::info("Edit the file or use 'ragcli config set KEY VALUE' to configure.");
        return 0;
    }

    // --- 유틸리티 ---

    static auto is_env_file(const std::string &path) -> bool {
        return path.length() >= 4 && path.substr(path.length() - 4) == ".env";
    }

    // YAML/ENV 라인에서 키가 일치하는지 확인한다 ("KEY: value", "KEY=value" 지원)
    static auto matches_key(const std::string &line, const std::string &key) -> bool {
        const auto first = line.find_first_not_of(' ');
        if (first == std::string::npos || line[first] == '#') {
            return false;
        }
        if (line.size() < key.size()) {
            return false;
        }
        if (line.compare(first, key.size(), key) != 0) {
            return false;
        }
        const std::size_t after = first + key.size();
        if (after >= line.size()) {
            return false;
        }
        return line[after] == ':' || line[after] == '=';
    }

    // 파일 확장자에 따라 설정 줄을 생성한다 (.env -> KEY=VALUE, .yaml -> KEY: VALUE)
    static auto format_config_line(const std::string &path, const std::string &key, const std::string &value) -> std::string {
        if (is_env_file(path)) {
            return key + "=" + value;
        }
        // YAML 형식
        const bool needs_quote = value.find(':') != std::string::npos ||
                                 value.find('#') != std::string::npos ||
                                 value.find('"') != std::string::npos ||
                                 value.find('\'') != std::string::npos ||
                                 value.find(' ') != std::string::npos;
        if (needs_quote) {
            return key + ": \"" + value + "\"";
        }
        return key + ": " + value;
    }

    // ragcli config set KEY VALUE — 설정 값 변경
    static auto run_set(const wcppcli::Command &cmd) -> int {
        if (cmd.args.size() < 2) {
            wcppcli::WLog::error("Usage: ragcli config set KEY VALUE");
            return 1;
        }

        const std::string &key = cmd.args[0];
        const std::string &value = cmd.args[1];

        // 설정 파일 찾기 (없으면 기본 경로에 생성)
        std::string config_path = ragcli::utils::find_config_path();
        if (config_path.empty()) {
            config_path = ragcli::utils::get_default_config_path();
            const std::string config_dir = ragcli::utils::get_config_dir();
            std::filesystem::create_directories(config_dir);
        }

        // 기존 파일 읽기
        std::vector<std::string> lines;
        bool key_found = false;

        if (std::filesystem::exists(config_path)) {
            std::ifstream infile(config_path);
            std::string line;
            while (std::getline(infile, line)) {
                // YAML/ENV 형식: "KEY: VALUE" 또는 "KEY=VALUE"
                if (matches_key(line, key)) {
                    lines.push_back(format_config_line(config_path, key, value));
                    key_found = true;
                } else {
                    lines.push_back(line);
                }
            }
        }

        // 키가 없으면 파일 끝에 추가
        if (!key_found) {
            lines.push_back(format_config_line(config_path, key, value));
        }

        // 파일 쓰기
        std::ofstream outfile(config_path);
        if (!outfile.is_open()) {
            wcppcli::WLog::error("Failed to write: " + config_path);
            return 1;
        }
        for (const auto &line : lines) {
            outfile << line << "\n";
        }

        wcppcli::WLog::success(key + " = " + value);
        wcppcli::WLog::info("Saved to: " + config_path);
        return 0;
    }

    // ragcli config path — 설정 파일 경로 출력
    static auto run_path() -> int {
        const std::string path = ragcli::utils::find_config_path();
        if (path.empty()) {
            // 파일이 없어도 기본 경로를 보여준다
            std::cout << ragcli::utils::get_default_config_path() << " (not created)\n";
        } else {
            std::cout << std::filesystem::absolute(path).string() << "\n";
        }
        return 0;
    }

    // 기본 config.yaml 내용 생성
    static auto generate_default_yaml() -> std::string {
        std::ostringstream oss;
        oss << "# ragcli 설정 파일\n"
            << "#\n"
            << "# 설정 파일 탐색 순서:\n"
            << "#   1) 플랫폼 설정 디렉토리의 config.yaml\n"
            << "#   2) 플랫폼 설정 디렉토리의 .env\n"
            << "#   3) CWD의 config.yaml\n"
            << "#   4) CWD의 .env\n"
            << "\n"
            << "# LLM 프로바이더: ollama (기본), openai, azure\n"
            << "LLM_PROVIDER: ollama\n"
            << "\n"
            << "# Ollama 설정\n"
            << "OLLAMA_BASE_URL: \"http://localhost:11434\"\n"
            << "OLLAMA_MODEL: llama3\n"
            << "OLLAMA_EMBED_MODEL: nomic-embed-text\n"
            << "\n"
            << "# OpenAI 설정\n"
            << "# OPENAI_API_KEY: \"your_openai_api_key_here\"\n"
            << "# OPENAI_BASE_URL: \"https://api.openai.com/v1\"\n"
            << "# OPENAI_MODEL: gpt-4o-mini\n"
            << "# OPENAI_EMBED_MODEL: text-embedding-3-small\n"
            << "\n"
            << "# Azure OpenAI 설정\n"
            << "# AZURE_OPENAI_API_KEY: \"your_azure_openai_api_key_here\"\n"
            << "# AZURE_OPENAI_BASE_URL: \"https://your-resource-name.openai.azure.com/\"\n"
            << "# AZURE_OPENAI_MODEL: your-chat-deployment-name\n"
            << "# AZURE_OPENAI_EMBED_MODEL: your-embedding-deployment-name\n"
            << "# AZURE_OPENAI_API_VERSION: \"2024-02-15-preview\"\n"
            << "\n"
            << "# Qdrant 설정\n"
            << "QDRANT_BASE_URL: \"http://localhost:6333\"\n"
            << "QDRANT_COLLECTION: documents\n"
            << "# QDRANT_DISTANCE: Cosine\n"
            << "\n"
            << "# 청킹 설정\n"
            << "# CHUNK_SIZE: 512\n"
            << "# CHUNK_OVERLAP: 64\n";
        return oss.str();
    }
};

} // namespace ragcli::cmd
