#include "rom/metadata/animation_metadata.h"

#include "json.hpp"

namespace spintool::metadata
{
    namespace
    {
        // Stable names for the JSON, so reordering the enum can't change existing files.
        constexpr std::array<const char*, static_cast<size_t>(AnimationPaletteSet::COUNT)> s_palette_set_ids
        {
            "global",
            "toxic_caves",
            "lava_powerhouse",
            "the_machine",
            "showdown",
            "main_menu",
            "intro_cutscene",
            "options_screen",
            "sega_logo",
        };
    }

    const char* AnimationPaletteSetToString(const AnimationPaletteSet palette_set)
    {
        const size_t index = static_cast<size_t>(palette_set);
        return index < s_animation_palette_set_names.size() ? s_animation_palette_set_names[index] : "Unknown";
    }

    AnimationPaletteSet AnimationPaletteSetFromString(const std::string& name)
    {
        for (size_t i = 0; i < s_palette_set_ids.size(); ++i)
        {
            if (name == s_palette_set_ids[i])
            {
                return static_cast<AnimationPaletteSet>(i);
            }
        }
        return AnimationMetadata::s_default_palette_set;
    }

    bool AnimationMetadata::IsEmpty() const
    {
        return name.empty() && palette_set == s_default_palette_set && palette_line == s_default_palette_line && ticks_per_frame == 0 && sprite_table == 0;
    }

    void AnimationMetadata::Serialise(nlohmann::json& writer) const
    {
        writer["rom_offset"] = rom_offset;
        writer["name"] = name;
        const size_t set_index = static_cast<size_t>(palette_set);
        writer["palette_set"] = set_index < s_palette_set_ids.size() ? s_palette_set_ids[set_index] : s_palette_set_ids[0];
        writer["palette_line"] = palette_line;
        writer["ticks_per_frame"] = ticks_per_frame;
        writer["sprite_table"] = sprite_table;
    }

    void AnimationMetadata::Deserialise(const nlohmann::json& reader)
    {
        rom_offset = reader.value("rom_offset", static_cast<Uint32>(0));
        name = reader.value("name", std::string{});
        palette_set = AnimationPaletteSetFromString(reader.value("palette_set", std::string{}));
        palette_line = reader.value("palette_line", s_default_palette_line);
        ticks_per_frame = reader.value("ticks_per_frame", static_cast<Uint8>(0));
        sprite_table = reader.value("sprite_table", static_cast<Uint32>(0));
    }
}
