#pragma once

#include "rom_asset_definitions.h"
#include "game_object_metadata.h"

#include "json_fwd.hpp"

#include <array>
#include <filesystem>
#include <limits>
#include <vector>


namespace spintool::metadata
{
    struct LevelMetadata
    {
        rom::LevelDataOffsets level_data_offsets;
        std::array<GameObjectMetadata, std::numeric_limits<Uint8>::max() + 1> game_object_metadata;

        void Serialise(nlohmann::json& writer);
        void Deserialise(const nlohmann::json& reader);

        void SerialiseGameObjects(nlohmann::json& writer);
        void DeserialiseGameObjects(const nlohmann::json& reader);
    };
}
