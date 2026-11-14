#pragma once

#include "rom_asset_definitions.h"
#include "level_metadata.h"
#include "animation_metadata.h"

#include "json_fwd.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <string>
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

        // The level data tables of this version, from metadata.json (USA) or
        // metadata_<version>.json. Returns false (keeping the USA defaults) if the
        // file is missing or unreadable.
        [[nodiscard]] static std::string TableMetadataFilename(const std::string& version_id);
        bool LoadTableMetadata(const std::filesystem::path& directory);
        // Makes this version's tables the ones LevelDataOffsets(level_index) uses.
        void ActivateTables();

        [[nodiscard]] GameObjectMetadata* GetGameObjectMetadata(int level_index, Uint8 type_id);
        void SaveGameObjectMetadata(const std::filesystem::path& directory);
        void LoadGameObjectMetadata(const std::filesystem::path& directory);

        // Animation metadata, keyed by the animation's ROM offset. Stored in animations_<version>.json.
        std::map<Uint32, AnimationMetadata> animation_metadata;
        [[nodiscard]] const AnimationMetadata* GetAnimationMetadata(Uint32 rom_offset) const;
        [[nodiscard]] AnimationMetadata& GetOrCreateAnimationMetadata(Uint32 rom_offset);
        void SaveAnimationMetadata(const std::filesystem::path& directory);
        void LoadAnimationMetadata(const std::filesystem::path& directory);
    };

    struct Metadata
    {
        std::vector<ROMMetadata> rom_metadatas;

        Metadata();

        ROMMetadata* GetROMMetadataFor(std::string_view version_id);
    };
}
