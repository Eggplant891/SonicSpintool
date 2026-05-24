#include "rom/metadata/rom_metadata.h"

#include "serialisation/editor_serialiser.h"

#include "json.hpp"

#include <algorithm>

namespace spintool::rom
{
    void ROMMetadata::Serialise(nlohmann::json& writer)
    {
        writer["version"] = version_id;
        writer["path"] = location_on_disk;
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
        version_id = reader["version"].get<std::string>();
        location_on_disk = reader["location_on_disk"].get<std::filesystem::path>();
        level_data_table_offsets.Deserialise(reader["data_tables"]);
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
}
