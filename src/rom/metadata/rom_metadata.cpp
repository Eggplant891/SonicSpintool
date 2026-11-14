#include "rom/metadata/rom_metadata.h"

#include "rom/rom_address_map.h"
#include "serialisation/editor_serialiser.h"

#include "json.hpp"

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <system_error>

namespace spintool::metadata
{
    namespace
    {
        std::string GameObjectMetadataFilename(const std::string& version_id)
        {
            return "game_objects_" + version_id + ".json";
        }

        std::string AnimationMetadataFilename(const std::string& version_id)
        {
            return "animations_" + version_id + ".json";
        }
    }

    GameObjectMetadata* ROMMetadata::GetGameObjectMetadata(const int level_index, const Uint8 type_id)
    {
        if (level_index < 0 || static_cast<size_t>(level_index) >= level_data_offsets.size())
        {
            return nullptr;
        }

        return &level_data_offsets[level_index].game_object_metadata[type_id];
    }

    void ROMMetadata::SaveGameObjectMetadata(const std::filesystem::path& directory)
    {
        // Written to a temporary file first, so a failed save can never damage the existing metadata.
        const std::string filename = GameObjectMetadataFilename(version_id);
        const std::string temp_filename = filename + ".tmp";
        try
        {
            std::unique_ptr<Serialiser> serialiser = Serialiser::OpenFile(directory, temp_filename);
            nlohmann::json& writer = serialiser->Writer();
            writer["version"] = version_id;
            nlohmann::json levels_writer = nlohmann::json::array();
            for (LevelMetadata& level_metadata : level_data_offsets)
            {
                level_metadata.SerialiseGameObjects(levels_writer.emplace_back());
            }
            writer["levels"] = levels_writer;
            serialiser->Finalise();
            std::filesystem::rename(directory / temp_filename, directory / filename);
        }
        catch (const std::exception& error)
        {
            std::cerr << "Failed to save game object metadata: " << error.what() << '\n';
        }
    }

    void ROMMetadata::LoadGameObjectMetadata(const std::filesystem::path& directory)
    {
        const std::string filename = GameObjectMetadataFilename(version_id);
        try
        {
            std::unique_ptr<Deserialiser> deserialiser = Deserialiser::OpenFile(directory, filename);
            if (deserialiser == nullptr)
            {
                return;
            }

            const nlohmann::json& levels_reader = deserialiser->Reader().at("levels");
            for (size_t i = 0; i < level_data_offsets.size() && i < levels_reader.size(); ++i)
            {
                level_data_offsets[i].DeserialiseGameObjects(levels_reader[i]);
            }
        }
        catch (const std::exception& error)
        {
            // Keep the unreadable file, otherwise the next save would replace it with an empty one.
            std::error_code error_code;
            std::filesystem::copy_file(directory / filename, directory / (filename + ".invalid"),
                std::filesystem::copy_options::overwrite_existing, error_code);
            std::cerr << "Failed to load game object metadata: " << error.what() << '\n';
        }
    }

    const AnimationMetadata* ROMMetadata::GetAnimationMetadata(const Uint32 rom_offset) const
    {
        const auto found = animation_metadata.find(rom_offset);
        return found != std::end(animation_metadata) ? &found->second : nullptr;
    }

    AnimationMetadata& ROMMetadata::GetOrCreateAnimationMetadata(const Uint32 rom_offset)
    {
        AnimationMetadata& entry = animation_metadata[rom_offset];
        entry.rom_offset = rom_offset;
        return entry;
    }

    void ROMMetadata::SaveAnimationMetadata(const std::filesystem::path& directory)
    {
        // Written to a temporary file first, so a failed save can never damage the existing metadata.
        const std::string filename = AnimationMetadataFilename(version_id);
        const std::string temp_filename = filename + ".tmp";
        try
        {
            std::unique_ptr<Serialiser> serialiser = Serialiser::OpenFile(directory, temp_filename);
            nlohmann::json& writer = serialiser->Writer();
            writer["version"] = version_id;
            nlohmann::json animations_writer = nlohmann::json::array();
            for (const auto& [rom_offset, entry] : animation_metadata)
            {
                if (entry.IsEmpty() == false)
                {
                    entry.Serialise(animations_writer.emplace_back());
                }
            }
            writer["animations"] = animations_writer;
            serialiser->Finalise();
            std::filesystem::rename(directory / temp_filename, directory / filename);
        }
        catch (const std::exception& error)
        {
            std::cerr << "Failed to save animation metadata: " << error.what() << '\n';
        }
    }

    void ROMMetadata::LoadAnimationMetadata(const std::filesystem::path& directory)
    {
        const std::string filename = AnimationMetadataFilename(version_id);
        try
        {
            std::unique_ptr<Deserialiser> deserialiser = Deserialiser::OpenFile(directory, filename);
            if (deserialiser == nullptr)
            {
                return;
            }

            animation_metadata.clear();
            for (const nlohmann::json& entry_reader : deserialiser->Reader().at("animations"))
            {
                AnimationMetadata entry;
                entry.Deserialise(entry_reader);
                animation_metadata[entry.rom_offset] = std::move(entry);
            }
        }
        catch (const std::exception& error)
        {
            // Keep the unreadable file, otherwise the next save would replace it with an empty one.
            std::error_code error_code;
            std::filesystem::copy_file(directory / filename, directory / (filename + ".invalid"),
                std::filesystem::copy_options::overwrite_existing, error_code);
            std::cerr << "Failed to load animation metadata: " << error.what() << '\n';
        }
    }

    void ROMMetadata::Serialise(nlohmann::json& writer)
    {
        writer["version"] = version_id;
        writer["path"] = location_on_disk.string();
        level_data_table_offsets.Serialise(writer["data_tables"]);

        {
            auto array_writer = writer["level_data_offsets"].array();
            for (auto& array_count_entry : level_data_offsets)
            {
                auto& array_entry_writer = array_writer.emplace_back();
                array_count_entry.Serialise(array_entry_writer);
            }
            writer["level_data_offsets"] = array_writer;
        }
    }

    void ROMMetadata::Deserialise(const nlohmann::json& reader)
    {
        if (const auto version_it = reader.find("version");
            version_it != reader.end() && version_it->is_string())
        {
            version_id = version_it->get<std::string>();
        }
        else
        {
            version_id.clear();
        }

        location_on_disk.clear();

        // Current metadata files use "path".
        if (const auto path_it = reader.find("path");
            path_it != reader.end() && path_it->is_string())
        {
            location_on_disk =
                std::filesystem::path{path_it->get<std::string>()};
        }
        // Backward compatibility with older/generated metadata files.
        else if (const auto path_it = reader.find("location_on_disk");
                 path_it != reader.end() && path_it->is_string())
        {
            location_on_disk =
                std::filesystem::path{path_it->get<std::string>()};
        }

        if (const auto tables_it = reader.find("data_tables");
            tables_it != reader.end() && tables_it->is_object())
        {
            level_data_table_offsets.Deserialise(*tables_it);
        }
    }

    std::string ROMMetadata::TableMetadataFilename(const std::string& version_id)
    {
        return version_id == "usa" ? std::string{"metadata.json"} : "metadata_" + version_id + ".json";
    }

    bool ROMMetadata::LoadTableMetadata(const std::filesystem::path& directory)
    {
        // The address relocations of each group (rom_address_map.h)
        rom::ClearAddressRelocations();
        for (const rom::AddressGroup group : { rom::AddressGroup::Common, rom::AddressGroup::BonusStage, rom::AddressGroup::Frontend })
        {
            const std::string address_file = rom::AddressFilename(group, version_id);
            if (!rom::LoadAddressRelocations(group, directory / address_file) && version_id != "usa")
            {
                std::cerr << "No ROM addresses for " << version_id << " in " << address_file
                    << ": its features use the USA ROM's addresses\n";
            }
        }

        level_data_table_offsets = rom::LevelDataTableOffsets{};
        const std::string filename = TableMetadataFilename(version_id);
        if (!Deserialiser::FileExists(directory / filename))
        {
            return false;
        }

        try
        {
            std::unique_ptr<Deserialiser> deserialiser = Deserialiser::OpenFile(directory, filename);
            if (deserialiser == nullptr)
            {
                return false;
            }

            const nlohmann::json& reader = deserialiser->Reader();
            const auto tables_it = reader.find("data_tables");
            if (tables_it == reader.end() || !tables_it->is_object())
            {
                std::cerr << filename << " has no data_tables\n";
                return false;
            }

            level_data_table_offsets.Deserialise(*tables_it);
            return true;
        }
        catch (const std::exception& error)
        {
            std::cerr << "Failed to load " << filename << ": " << error.what() << '\n';
            level_data_table_offsets = rom::LevelDataTableOffsets{};
            return false;
        }
    }

    void ROMMetadata::ActivateTables()
    {
        rom::RelocateGlobalAddresses();
        rom::LevelDataTableOffsets::SetActive(level_data_table_offsets);
        for (size_t i = 0; i < level_data_offsets.size(); ++i)
        {
            level_data_offsets[i].level_data_offsets = rom::LevelDataOffsets{static_cast<int>(i)};
        }
    }

    ROMMetadata::ROMMetadata()
    {
        for (size_t i = 0; i < level_data_offsets.size(); ++i)
        {
            level_data_offsets[i] = LevelMetadata{i};
        }
    }

    ROMMetadata* Metadata::GetROMMetadataFor(std::string_view version_id)
    {
        auto result_it = std::find_if(std::begin(rom_metadatas), std::end(rom_metadatas)
            , [&version_id](const ROMMetadata& entry)
            {
                return entry.version_id == version_id;
            });

        if (result_it != std::end(rom_metadatas))
        {
            return &(*result_it);
        }

        return nullptr;
    }

    Metadata::Metadata()
    {
        rom_metadatas.emplace_back().version_id = "usa";
        rom_metadatas.emplace_back().version_id = "eur";
        rom_metadatas.emplace_back().version_id = "jp";
    }
}
