#pragma once

#include "rom_asset_definitions.h"

#include <array>
#include <filesystem>
#include <vector>

#include "json_fwd.hpp"

namespace spintool::rom
{
    struct GameObjectMetadata
    {
        Uint8 type_id;
        std::string type_name;

        void Serialise(nlohmann::json& writer);
        void Deserialise(const nlohmann::json &reader);
    };

    struct LevelMetadata
    {
        LevelDataOffsets level_data_offsets;
        std::array<GameObjectMetadata, std::numeric_limits<Uint8>::max()> game_object_metadata;

        void Serialise(nlohmann::json& writer);
        void Deserialise(const nlohmann::json &reader);
    };

    struct ROMMetadata
    {
        ROMMetadata();

        std::string version_id = "unknown";
        std::filesystem::path location_on_disk;
        LevelDataTableOffsets level_data_table_offsets;
        std::array<LevelMetadata, 4> level_data_offsets;

        void Serialise(nlohmann::json& writer);
        void Deserialise(const nlohmann::json &reader);
    };

    struct Metadata
    {
        std::vector<ROMMetadata> rom_metadatas;

        Metadata();

        ROMMetadata* GetROMMetadataFor(std::string_view version_id);
    };
}
