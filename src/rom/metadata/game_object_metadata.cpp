#include "rom/metadata/game_object_metadata.h"

#include "json.hpp"

namespace spintool::metadata
{
    bool GameObjectMetadata::IsEmpty() const
    {
        return type_name.empty() && notes.empty() && unk_1_name.empty() && unk_2_name.empty() && flags.empty();
    }

    void GameObjectMetadata::Serialise(nlohmann::json& writer)
    {
        writer["type_id"] = type_id;
        writer["type_name"] = type_name;
        writer["notes"] = notes;

        writer["unk_1_name"] = unk_1_name;
        writer["unk_2_name"] = unk_2_name;

        nlohmann::json flags_writer = nlohmann::json::array();
        for (const GameObjectFlagMetadata& flag : flags)
        {
            nlohmann::json& flag_writer = flags_writer.emplace_back();
            flag_writer["bit"] = flag.bit;
            flag_writer["name"] = flag.name;
        }
        writer["flags"] = flags_writer;
    }

    void GameObjectMetadata::Deserialise(const nlohmann::json& reader)
    {
        type_id = reader.value("type_id", static_cast<Uint8>(0));
        type_name = reader.value("type_name", std::string{});
        notes = reader.value("notes", std::string{});

        unk_1_name = reader.value("unk_1_name", std::string{});
        unk_2_name = reader.value("unk_2_name", std::string{});

        flags.clear();
        if (const auto flags_it = reader.find("flags");
            flags_it != reader.end() && flags_it->is_array())
        {
            for (const nlohmann::json& flag_reader : *flags_it)
            {
                GameObjectFlagMetadata& flag = flags.emplace_back();
                flag.bit = flag_reader.value("bit", static_cast<Uint8>(0));
                flag.name = flag_reader.value("name", std::string{});
            }
        }
    }
}
