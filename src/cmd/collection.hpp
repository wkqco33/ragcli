#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wlog.hpp>

#include "command.hpp"
#include "flag_helper.hpp"
#include "qdrant/qdrant_client.hpp"
#include "wcppcli/wconf.hpp"

namespace ragcli::cmd {

// `ragcli collection` 서브커맨드.
// Qdrant 컬렉션 목록 조회, 상세 정보 및 삭제 관리를 수행한다.
class CollectionCommand : public CommandBase {
  public:
    void register_to(wcppcli::Command &root) override {
        auto cmd = std::make_unique<wcppcli::Command>();
        cmd->name = "collection";
        cmd->description = "Manage Qdrant vector collections (list, info, delete)";

        add_bool_flag(*cmd, "list", 'l', "List all Qdrant collections", &list_);
        add_bool_flag(*cmd, "info", 'i', "Get info of the specified Qdrant collection", &info_);
        add_bool_flag(*cmd, "delete", 'd', "Delete the specified Qdrant collection", &delete_);
        add_string_flag(*cmd, "collection", 'c', "Target collection name", &collection_);
        add_string_flag(*cmd, "qdrant-url", 0, "Qdrant base URL (default: http://localhost:6333)",
                        &qdrant_url_);

        cmd->handler = [this](const wcppcli::Command & /*unused*/) { return run_collection(); };

        root.add_command(std::move(cmd));
    }

  private:
    auto run_collection() -> int {
        wcppcli::WConf conf;
        conf.read_file(".env");

        std::string url = !qdrant_url_.empty() ? qdrant_url_ : conf.get_string("QDRANT_BASE_URL");
        if (url.empty()) {
            url = "http://localhost:6333";
        }

        std::string target_collection =
            !collection_.empty() ? collection_ : conf.get_string("QDRANT_COLLECTION");
        if (target_collection.empty()) {
            target_collection = "documents";
        }

        ragcli::qdrant::QdrantClient client(url, target_collection);

        try {
            if (list_) {
                wcppcli::WLog::info("Fetching collection list from " + url + "...");
                auto res = client.list_collections();
                std::cout << res.dump(2) << std::endl;
                return 0;
            }

            if (info_) {
                wcppcli::WLog::info("Fetching info for collection '" + target_collection + "'...");
                auto res = client.get_collection_info();
                std::cout << res.dump(2) << std::endl;
                return 0;
            }

            if (delete_) {
                wcppcli::WLog::info("Deleting collection '" + target_collection + "'...");
                client.delete_collection();
                wcppcli::WLog::success("Collection '" + target_collection + "' deleted.");
                return 0;
            }

            wcppcli::WLog::error(
                "Please specify one of --list (-l), --info (-i), or --delete (-d).");
            return 1;

        } catch (const std::exception &e) {
            wcppcli::WLog::error("Collection operation failed: " + std::string(e.what()));
            return 1;
        }
    }

    bool list_ = false;
    bool info_ = false;
    bool delete_ = false;
    std::string collection_;
    std::string qdrant_url_;
};

} // namespace ragcli::cmd
