#pragma once

#include "rom_asset_definitions.h"

#include "json_fwd.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <vector>


namespace spintool::metadata
{
    struct GameObjectFlagMetadata
    {
        Uint8 bit = 0;
        std::string name;
    };

    // User-authored information about a game object type. None of this exists in the ROM.
    struct GameObjectMetadata
    {
        Uint8 type_id = 0;
        std::string type_name;
        std::string notes;
        std::string unk_1_name;
        std::string unk_2_name;
        std::vector<GameObjectFlagMetadata> flags;

        [[nodiscard]] bool IsEmpty() const;

        void Serialise(nlohmann::json& writer);
        void Deserialise(const nlohmann::json &reader);
    };
}
