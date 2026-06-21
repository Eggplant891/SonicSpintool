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

	enum class TailsPlaneScene : Uint8
	{
		IntroBeforeTitle = 0,
		AfterStart = 1,
		BeforeDemo = 2,
		EndingCutscene = 3,
	};

	struct TailsPlaneSceneInfo
	{
		TailsPlaneScene scene = TailsPlaneScene::IntroBeforeTitle;
		std::string name;
		std::string asm_path;
		std::string note;
	};

	struct TailsPlaneFrame
	{
		std::string name;
		std::string usage;
		std::size_t frame_id = 0;
		Uint8 scene_mask = 0;
		std::shared_ptr<const Sprite> sprite;
	};

	struct TailsPlaneDecodeResult
	{
		std::vector<TailsPlaneFrame> frames;
		std::vector<std::string> warnings;
		std::string error;

		[[nodiscard]] bool Succeeded() const
		{
			return error.empty() && !frames.empty();
		}
	};

	struct TailsPlaneImportResult
	{
		bool success = false;
		std::string message;
	};

	class TailsPlaneDecoder
	{
	public:
		static const std::vector<TailsPlaneSceneInfo>& GetSceneInfos();

		// Decodes only Tails' plane. The scene mask keeps shared profile frames
		// in memory once while still filtering the correct frames per sequence.
		static TailsPlaneDecodeResult Decode(const SpinballROM& rom);

		// Imports one complete assembled frame selected by its stable frame ID.
		// All four sequences read the same Compressed2 graphics block, but their
		// mappings are kept separate and are never presented as fake duplicates.
		static TailsPlaneImportResult ImportIndexedImage(
			SpinballROM& rom,
			const SpinballROM& reference_rom,
			std::size_t frame_id,
			const std::vector<Uint8>& indexed_pixels,
			int image_width,
			int image_height
		);
	};
}
