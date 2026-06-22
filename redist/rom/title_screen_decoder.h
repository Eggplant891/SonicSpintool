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
	struct PaletteSet;

	enum class TitleScreenCategory : Uint8
	{
		SONIC = 0,
		BUMPER_RING = 1,
		LOGO_SONIC = 2,
		LOGO_THE_HEDGEHOG = 3,
		LOGO_SPINBALL = 4,
	};

	struct TitleScreenFrame
	{
		TitleScreenCategory category = TitleScreenCategory::SONIC;
		std::string name;
		std::string usage;
		std::size_t frame_id = 0;
		std::shared_ptr<const Sprite> sprite;
		// Palette line used by the owning mapping piece for each output pixel.
		// This also preserves the correct line for transparent pixels painted
		// during PNG import. Values are always in the range 0-3.
		std::vector<Uint8> palette_line_map;
	};

	struct TitleScreenDecodeResult
	{
		std::vector<TitleScreenFrame> frames;
		std::shared_ptr<const PaletteSet> palette_set;
		std::vector<std::string> warnings;
		std::string error;

		[[nodiscard]] bool Succeeded() const
		{
			return error.empty() && !frames.empty();
		}
	};

	struct TitleScreenImportResult
	{
		bool success = false;
		bool changed = false;
		std::string message;
	};

	class TitleScreenDecoder
	{
	public:
		// Replays the title-screen object scripts and reconstructs the complete
		// sprite composition at each wait/capture step.
		static TitleScreenDecodeResult Decode(const SpinballROM& rom);

		// Imports one assembled frame into the shared title-screen Compressed2
		// graphics block. The reference ROM defines the maximum writable span.
		static TitleScreenImportResult ImportIndexedImage(
			SpinballROM& rom,
			const SpinballROM& reference_rom,
			std::size_t frame_id,
			const std::vector<Uint8>& indexed_pixels,
			int image_width,
			int image_height
		);
	};
}
