#pragma once

#include "rom_asset_definitions.h"
#include "level_metadata.h"

#include "json_fwd.hpp"

#include <array>
#include <filesystem>
#include <vector>


namespace spintool::metadata
{
    struct ROMMetadata
    {
        ROMMetadata();

        std::string version_id = "unknown";
        std::filesystem::path location_on_disk;
        rom::LevelDataTableOffsets level_data_table_offsets;
        std::array<LevelMetadata, 4> level_data_offsets;

        void Serialise(nlohmann::json& writer);
        void Deserialise(const nlohmann::json &reader);

        [[nodiscard]] GameObjectMetadata* GetGameObjectMetadata(int level_index, Uint8 type_id);
        void SaveGameObjectMetadata(const std::filesystem::path& directory);
        void LoadGameObjectMetadata(const std::filesystem::path& directory);
    };

    struct Metadata
    {
        std::vector<ROMMetadata> rom_metadatas;

        Metadata();

        ROMMetadata* GetROMMetadataFor(std::string_view version_id);
    };
}
