#include "rom/metadata/level_metadata.h"

#include "json.hpp"

namespace spintool::metadata
{
    void LevelMetadata::Serialise(nlohmann::json& writer)
    {
        level_data_offsets.Serialise(writer);
    }

    void LevelMetadata::Deserialise(const nlohmann::json& reader)
    {
        reader;
    }

    void LevelMetadata::SerialiseGameObjects(nlohmann::json& writer)
    {
        writer = nlohmann::json::array();
        for (size_t type_id = 0; type_id < game_object_metadata.size(); ++type_id)
        {
            GameObjectMetadata& obj_metadata = game_object_metadata[type_id];
            if (obj_metadata.IsEmpty() == false)
            {
                obj_metadata.type_id = static_cast<Uint8>(type_id);
                obj_metadata.Serialise(writer.emplace_back());
            }
        }
    }

    void LevelMetadata::DeserialiseGameObjects(const nlohmann::json& reader)
    {
        for (const nlohmann::json& obj_reader : reader)
        {
            GameObjectMetadata obj_metadata;
            obj_metadata.Deserialise(obj_reader);
            game_object_metadata[obj_metadata.type_id] = std::move(obj_metadata);
        }
    }
}
