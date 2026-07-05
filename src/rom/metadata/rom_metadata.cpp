#include "rom/metadata/rom_metadata.h"

#include "serialisation/editor_serialiser.h"

#include "json.hpp"

#include <algorithm>

namespace spintool::rom
{
    void ROMMetadata::Serialise(nlohmann::json& writer)
    {
        writer["version"] = version_id;
        writer["path"] = location_on_disk.string();
        level_data_table_offsets.Serialise(writer["data_tables"]);

        {
            auto array_writer = writer["level_data_offsets"].array();
            for (auto& array_count_entry : level_data_offsets)
            {
                auto& array_entry_writer = array_writer.emplace_back();
                array_count_entry.Serialise(array_entry_writer);
            }
            writer["level_data_offsets"] = array_writer;
        }
    }

    void ROMMetadata::Deserialise(const nlohmann::json& reader)
    {
        if (const auto version_it = reader.find("version");
            version_it != reader.end() && version_it->is_string())
        {
            version_id = version_it->get<std::string>();
        }
        else
        {
            version_id.clear();
        }

        location_on_disk.clear();

        // Current metadata files use "path".
        if (const auto path_it = reader.find("path");
            path_it != reader.end() && path_it->is_string())
        {
            location_on_disk =
                std::filesystem::path{path_it->get<std::string>()};
        }
        // Backward compatibility with older/generated metadata files.
        else if (const auto path_it = reader.find("location_on_disk");
                 path_it != reader.end() && path_it->is_string())
        {
            location_on_disk =
                std::filesystem::path{path_it->get<std::string>()};
        }

        if (const auto tables_it = reader.find("data_tables");
            tables_it != reader.end() && tables_it->is_object())
        {
            level_data_table_offsets.Deserialise(*tables_it);
        }
    }

    void GameObjectMetadata::Serialise(nlohmann::json& writer)
    {
        writer;
    }

    void GameObjectMetadata::Deserialise(const nlohmann::json& reader)
    {
        reader;
    }

    void LevelMetadata::Serialise(nlohmann::json& writer)
    {
        level_data_offsets.Serialise(writer);
    }

    void LevelMetadata::Deserialise(const nlohmann::json& reader)
    {
        reader;
    }

    ROMMetadata::ROMMetadata()
    {
        for (size_t i = 0; i < level_data_offsets.size(); ++i)
        {
            level_data_offsets[i] = LevelMetadata{i};
        }
    }

    ROMMetadata* Metadata::GetROMMetadataFor(std::string_view version_id)
    {
        auto result_it = std::find_if(std::begin(rom_metadatas), std::end(rom_metadatas), [&version_id](const ROMMetadata& entry)
        {
            return entry.version_id == version_id;
        });

        if (result_it != std::end(rom_metadatas))
        {
            return &(*result_it);
        }

        return nullptr;
    }

    Metadata::Metadata()
    {
        rom_metadatas.emplace_back().version_id = "usa";
        rom_metadatas.emplace_back().version_id = "eur";
        rom_metadatas.emplace_back().version_id = "jp";
    }
}
