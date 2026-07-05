#pragma once

#include "SDL3/SDL_stdinc.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace spintool::rom
{
	class SpinballROM;
	struct Sprite;

	enum class BonusStageId : Uint8
	{
		RoboSmile = 0,
		CluckersDefense = 1,
		TrappedAlive = 2,
		TheMarch = 3,
	};

	struct BonusStageFrame
	{
		Uint32 animation_offset = 0;
		Uint32 mapping_offset = 0;
		Uint8 duration = 0;
		Uint8 command = 0;
		std::shared_ptr<const Sprite> sprite;
	};

	struct BonusStageAnimationState
	{
		std::string name;
		Uint32 animation_root = 0;
		bool is_static_mapping = false;
		std::vector<BonusStageFrame> frames;
	};

	struct BonusStageObjectFrames
	{
		std::string name;
		Uint32 object_ram_address = 0;
		Uint32 initial_mapping_offset = 0;
		Uint16 base_tile_attributes = 0;
		Uint8 palette_line = 0;
		bool is_common_object = false;
		bool high_priority = false;
		bool flip_horizontal = false;
		bool flip_vertical = false;
		std::vector<BonusStageAnimationState> states;

		[[nodiscard]] std::size_t GetFrameCount() const;
	};

	struct BonusStageDecodeResult
	{
		std::string stage_name;
		std::vector<BonusStageObjectFrames> objects;
		std::vector<std::string> warnings;
		std::string error;

		[[nodiscard]] std::size_t GetFrameCount() const;
		[[nodiscard]] std::size_t GetStateCount() const;

		[[nodiscard]] bool Succeeded() const;
	};

	struct BonusStageImportResult
	{
		bool success = false;
		bool changed = false;
		std::string message;
		std::vector<Uint32> rewritten_art_offsets;
	};

	class BonusStageDecoder
	{
	public:
		static const char* GetStageName(BonusStageId stage);
		static bool IsBonusStageMappingOffset(Uint32 rom_offset);

		// Reconstructs every known Bonus Stage object instance. Duplicate
		// instances and duplicate animation entries are intentionally retained.
		static BonusStageDecodeResult DecodeObjectFrames(
			const SpinballROM& rom,
			BonusStageId stage,
			std::size_t maximum_animation_entries = 2048
		);

		// Writes indexed PNG pixels back into the Compressed2 art referenced by
		// this mapping. The replacement is encoded with the game-compatible LZW
		// format, validated independently, and written strictly at the current
		// block address without changing the loader pointer or ROM size.
		static BonusStageImportResult ImportIndexedImage(
			SpinballROM& rom,
			const SpinballROM& reference_rom,
			BonusStageId stage,
			Uint32 mapping_offset,
			Uint16 visual_tile_attributes,
			const std::vector<Uint8>& indexed_pixels,
			int image_width,
			int image_height
		);

		[[nodiscard]] static BonusStageDecodeResult Decode(
			const SpinballROM& rom,
			BonusStageId stage,
			std::size_t maximum_animation_entries = 2048
		);
	};
}
