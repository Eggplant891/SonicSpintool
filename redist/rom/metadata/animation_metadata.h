#pragma once

#include "SDL3/SDL_stdinc.h"

#include "json_fwd.hpp"

#include <array>
#include <string>

namespace spintool::metadata
{
    // Which palettes an animation's sprites are drawn with in the game.
    enum class AnimationPaletteSet : Uint8
    {
        GLOBAL_PALETTES = 0,    // The palette line is an index into the ROM's palette array ($DFC)
        TOXIC_CAVES,
        LAVA_POWERHOUSE,
        THE_MACHINE,
        SHOWDOWN,
        MAIN_MENU,
        INTRO_CUTSCENE,
        OPTIONS_SCREEN,
        SEGA_LOGO,
        COUNT
    };

    inline constexpr std::array<const char*, static_cast<size_t>(AnimationPaletteSet::COUNT)> s_animation_palette_set_names
    {
        "Global palettes",
        "Toxic Caves",
        "Lava Powerhouse",
        "The Machine",
        "Showdown",
        "Main menu",
        "Intro cutscene",
        "Options screen",
        "SEGA logo",
    };

    [[nodiscard]] const char* AnimationPaletteSetToString(AnimationPaletteSet palette_set);
    [[nodiscard]] AnimationPaletteSet AnimationPaletteSetFromString(const std::string& name);

    // User-authored information about one animation, keyed by the animation's ROM offset. None of this exists in the ROM.
    struct AnimationMetadata
    {
        static constexpr AnimationPaletteSet s_default_palette_set = AnimationPaletteSet::GLOBAL_PALETTES;
        static constexpr Uint8 s_default_palette_line = 2; // What the animation loader has always used

        Uint32 rom_offset = 0;
        std::string name;
        AnimationPaletteSet palette_set = s_default_palette_set;
        Uint8 palette_line = s_default_palette_line;
        // Initial ticks per frame (the object definition's frame_time_ticks; SET_ANIM_TICKS_PER_FRAME can change it). 0 = not set.
        Uint8 ticks_per_frame = 0;
        // The sprite table the frames index (the object definition's sprite table). 0 = the table the animation list
        // was loaded with (the level's or the player's). Animations in a level's table can also use the shared table.
        Uint32 sprite_table = 0;

        [[nodiscard]] bool IsEmpty() const;
        [[nodiscard]] static bool UsesPaletteIndex(AnimationPaletteSet palette_set) { return palette_set == AnimationPaletteSet::GLOBAL_PALETTES; }

        void Serialise(nlohmann::json& writer) const;
        void Deserialise(const nlohmann::json& reader);
    };
}
