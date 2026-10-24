#pragma once

#include "rom_asset_definitions.h"

#include "json_fwd.hpp"

#include <array>
#include <filesystem>
#include <vector>


namespace spintool::metadata
{
    struct GameObjectMetadata
    {
        Uint8 type_id;
        std::string type_name;

        void Serialise(nlohmann::json& writer);
        void Deserialise(const nlohmann::json &reader);
    };
}
