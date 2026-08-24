#pragma once

#include <memory>
#include <vector>
#include <wcppcli/wcli.hpp>

#include "chat.hpp"
#include "collection.hpp"
#include "command.hpp"
#include "config.hpp"
#include "ocr.hpp"
#include "pdf_summarize.hpp"
#include "rag.hpp"
#include "version.hpp"

namespace ragcli::cmd {

// 모든 서브커맨드를 생성해 루트에 등록하고 상태 홀더를 반환한다. 홀더는
// root.execute() 가 끝날 때까지 살아있어야 한다 (value_ptr / this 캡처 수명).
// 새 커맨드 추가 시 holders.emplace_back(make_unique<...>()) 한 줄 추가.
inline auto register_commands(wcppcli::Command &root) -> std::vector<std::unique_ptr<CommandBase>> {
    root.version = ragcli::version;
    std::vector<std::unique_ptr<CommandBase>> holders;
    holders.emplace_back(std::make_unique<ChatCommand>());
    holders.emplace_back(std::make_unique<RagCommand>());
    holders.emplace_back(std::make_unique<CollectionCommand>());
    holders.emplace_back(std::make_unique<PdfSummarizeCommand>());
    holders.emplace_back(std::make_unique<OcrCommand>());
    holders.emplace_back(std::make_unique<ConfigCommand>());

    for (auto &cmd : holders) {
        cmd->register_to(root);
    }
    return holders;
}

} // namespace ragcli::cmd
