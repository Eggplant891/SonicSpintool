#include "rom/metadata/level_metadata.h"
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
}
