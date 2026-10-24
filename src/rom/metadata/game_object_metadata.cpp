#include "rom/metadata/game_object_metadata.h"

namespace spintool::metadata
{
    void GameObjectMetadata::Serialise(nlohmann::json& writer)
    {
        writer;
    }

    void GameObjectMetadata::Deserialise(const nlohmann::json& reader)
    {
        reader;
    }
}
