#include "rom/metadata/animation_palette_scraper.h"

#include "rom/animated_object.h"
#include "rom/animation_sequence.h"
#include "rom/game_objects/game_object_definition.h"
#include "rom/rom_asset_definitions.h"
#include "rom/spinball_rom.h"

#include <algorithm>

namespace spintool::metadata
{
	ScrapedAnimationPalette ScrapedAnimationUsage::MostCommon() const
	{
		const auto most_common = std::max_element(std::begin(palettes), std::end(palettes),
			[](const auto& lhs, const auto& rhs)
			{
				return lhs.second < rhs.second;
			});
		return most_common != std::end(palettes) ? most_common->first : ScrapedAnimationPalette{};
	}

	Uint8 ScrapedAnimationUsage::MostCommonTicksPerFrame() const
	{
		const auto most_common = std::max_element(std::begin(ticks_per_frame), std::end(ticks_per_frame),
			[](const auto& lhs, const auto& rhs)
			{
				return lhs.second < rhs.second;
			});
		return most_common != std::end(ticks_per_frame) ? most_common->first : 0;
	}

	Uint32 ScrapedAnimationUsage::MostCommonSpriteTable() const
	{
		const auto most_common = std::max_element(std::begin(sprite_tables), std::end(sprite_tables),
			[](const auto& lhs, const auto& rhs)
			{
				return lhs.second < rhs.second;
			});
		return most_common != std::end(sprite_tables) ? most_common->first : 0;
	}

	std::map<Uint32, ScrapedAnimationUsage> ScrapeAnimationPalettes(const rom::SpinballROM& rom)
	{
		std::map<Uint32, ScrapedAnimationUsage> result;
		const Uint32 rom_size = static_cast<Uint32>(rom.m_buffer.size());
		const auto in_rom = [rom_size](const Uint32 offset, const Uint32 size)
		{
			return offset != 0 && offset < rom_size && size <= rom_size - offset;
		};

		constexpr int s_level_count = 4;
		for (int level_index = 0; level_index < s_level_count; ++level_index)
		{
			const rom::LevelDataOffsets offsets{ level_index };
			const auto palette_set = static_cast<AnimationPaletteSet>(static_cast<int>(AnimationPaletteSet::TOXIC_CAVES) + level_index);

			// The level's animation table: these are the animations the game picks by index.
			std::set<Uint32> level_animations;
			if (in_rom(offsets.animations, 4))
			{
				const Uint32 table = rom.ReadUint32(offsets.animations);
				const Uint32 count = offsets.animation_count;
				for (Uint32 i = 0; i < count && in_rom(table + i * 4, 4); ++i)
				{
					level_animations.insert(rom.ReadUint32(table + i * 4));
				}
			}

			// Animated object definitions used by the level.
			std::vector<Uint32> definitions_to_visit;
			if (in_rom(offsets.flipper_object_definition, 4))
			{
				definitions_to_visit.push_back(rom.ReadUint32(offsets.flipper_object_definition));
			}
			Uint32 instance_offset = offsets.object_instances.offset;
			for (Uint32 i = 0; i < offsets.object_instances.count && in_rom(instance_offset, rom::GameObjectDefinition::s_size_on_rom); ++i)
			{
				const rom::GameObjectDefinition instance = rom::GameObjectDefinition::LoadFromROM(rom, instance_offset);
				if (instance.animation_definition != 0)
				{
					definitions_to_visit.push_back(instance.animation_definition);
				}
				instance_offset = std::max(instance.rom_data.rom_offset_end, instance_offset + 1);
			}

			// The flipper's other animations (idle, press, held, release) come from a table, drawn with the flipper definition's palette.
			if (in_rom(offsets.flipper_object_definition, 4) && in_rom(offsets.flipper_animations, 4))
			{
				const Uint32 flipper_definition_offset = rom.ReadUint32(offsets.flipper_object_definition);
				const Uint32 flipper_animation_table = rom.ReadUint32(offsets.flipper_animations);
				if (in_rom(flipper_definition_offset, rom::AnimObjectDefinition::s_size_on_rom) && in_rom(flipper_animation_table, 16))
				{
					const rom::AnimObjectDefinition flipper_definition = rom::AnimObjectDefinition::LoadFromROM(rom, flipper_definition_offset);
					const ScrapedAnimationPalette palette{ palette_set, static_cast<Uint8>(flipper_definition.palette_index % rom::s_max_palettes) };
					constexpr Uint32 s_flipper_animation_count = 4;
					for (Uint32 i = 0; i < s_flipper_animation_count; ++i)
					{
						const Uint32 animation = rom.ReadUint32(flipper_animation_table + i * 4);
						if (in_rom(animation, 2))
						{
							ScrapedAnimationUsage& usage = result[animation];
							++usage.palettes[palette];
							if (flipper_definition.frame_time_ticks != 0)
							{
								++usage.ticks_per_frame[flipper_definition.frame_time_ticks];
							}
							if (in_rom(flipper_definition.sprite_table, 4))
							{
								++usage.sprite_tables[rom.ReadUint32(flipper_definition.sprite_table)];
							}
						}
					}
				}
			}

			std::set<Uint32> visited_definitions;
			while (definitions_to_visit.empty() == false)
			{
				const Uint32 definition_offset = definitions_to_visit.back();
				definitions_to_visit.pop_back();
				if (in_rom(definition_offset, rom::AnimObjectDefinition::s_size_on_rom) == false || visited_definitions.insert(definition_offset).second == false)
				{
					continue;
				}

				const rom::AnimObjectDefinition definition = rom::AnimObjectDefinition::LoadFromROM(rom, definition_offset);
				if (in_rom(definition.starting_animation, 2) == false)
				{
					continue;
				}

				const ScrapedAnimationPalette palette{ palette_set, static_cast<Uint8>(definition.palette_index % rom::s_max_palettes) };
				// No sprite table: only the control flow is needed, not the frames' sprites.
				const auto sequence = rom::AnimationSequence::LoadFromROM(rom, definition.starting_animation, 0);

				std::set<Uint32> animations_for_definition{ definition.starting_animation };
				for (const rom::AnimationCommand& command : sequence->command_sequence)
				{
					if (level_animations.count(command.rom_data.rom_offset) != 0)
					{
						animations_for_definition.insert(command.rom_data.rom_offset);
					}
					if (command.command_type == rom::AnimationCommandType::CREATE_ANIM_OBJ_INSTANCE && command.pointer != 0)
					{
						definitions_to_visit.push_back(command.pointer);
					}
				}

				for (const Uint32 animation : animations_for_definition)
				{
					ScrapedAnimationUsage& usage = result[animation];
					++usage.palettes[palette];
					// 0 would mean 256 ticks in the game; no definition uses it, so treat it as "not known".
					if (definition.frame_time_ticks != 0)
					{
						++usage.ticks_per_frame[definition.frame_time_ticks];
					}
					if (in_rom(definition.sprite_table, 4))
					{
						++usage.sprite_tables[rom.ReadUint32(definition.sprite_table)];
					}
				}
			}
		}

		return result;
	}
}
