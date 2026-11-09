#pragma once

#include "rom/metadata/animation_metadata.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

namespace spintool::rom
{
	class SpinballROM;
}

namespace spintool::metadata
{
	// One way an animation is drawn in a level: the palette set and line of the animated object definition that uses it.
	struct ScrapedAnimationPalette
	{
		AnimationPaletteSet palette_set = AnimationPaletteSet::GLOBAL_PALETTES;
		Uint8 palette_line = 0;

		bool operator<(const ScrapedAnimationPalette& rhs) const
		{
			return std::pair{ palette_set, palette_line } < std::pair{ rhs.palette_set, rhs.palette_line };
		}
	};

	struct ScrapedAnimationUsage
	{
		// How many object definitions draw the animation with each palette. More than one entry means a conflict.
		std::map<ScrapedAnimationPalette, int> palettes;
		// The same for the definitions' frame_time_ticks (the initial ticks per frame).
		std::map<Uint8, int> ticks_per_frame;
		// And their sprite tables (the table itself, not the pointer to it).
		std::map<Uint32, int> sprite_tables;

		[[nodiscard]] ScrapedAnimationPalette MostCommon() const;
		[[nodiscard]] Uint8 MostCommonTicksPerFrame() const;
		[[nodiscard]] Uint32 MostCommonSpriteTable() const;
	};

	// Works out which palette each animation is drawn with, from the levels' object data:
	//   - every placed game object's animated object definition, the level's flipper definition, and any definition
	//     created by those definitions' animations (create-object commands)
	//   - each definition's starting animation, plus every entry in the level's animation table reachable from it
	// gets that level's palette set and the definition's palette line, frame_time_ticks and sprite table.
	// Animations that only the game's code switches to can't be found this way.
	[[nodiscard]] std::map<Uint32, ScrapedAnimationUsage> ScrapeAnimationPalettes(const rom::SpinballROM& rom);
}
