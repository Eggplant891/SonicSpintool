#include "ui/ui_tileset_navigator.h"

#include "ui/ui_editor.h"
#include "rom/spinball_rom.h"

namespace spintool
{
	EditorTilesetNavigator::EditorTilesetNavigator(EditorUI& owning_ui)
		: m_owning_ui(owning_ui)
		, m_rom(owning_ui.GetROM())
	{

	}

	size_t s_tile_offsets[] =
	{
		0x0003DBB2,
		0x000394AA,
		0x00067672,
		0x00064BB6,
		0x00081EB6,
		0x0007F29C,
		0x00053214,
		0x0004FEE4,
	};

	void EditorTilesetNavigator::Update()
	{
		if (visible == false)
		{
			return;
		}

		if (ImGui::Begin("Tileset Navigator", &visible))
		{
			if (ImGui::Button("Load all tilesets"))
			{
				for (size_t offset : s_tile_offsets)
				{
					m_tilesets.emplace_back(m_owning_ui.GetROM().LoadTileData(offset));
				}
			}

			for (const std::shared_ptr<const rom::TileSet>& tileset : m_tilesets)
			{
				char name_buffer[1024];
				sprintf_s(name_buffer, "Tileset(0x%06X -> 0x%06X)", static_cast<unsigned int>(tileset->rom_data.rom_offset), static_cast<unsigned int>(tileset->rom_data.rom_offset_end-1));
				ImGui::SeparatorText(name_buffer);
				ImGui::Text("Tiles: %02d (0x%02X)", tileset->num_tiles, tileset->num_tiles);
				ImGui::Text("Size on ROM: %02d (0x%04X)", tileset->rom_data.real_size, tileset->rom_data.real_size);
				ImGui::Text("Compressed Size: %02d (0x%04X)", tileset->compressed_size, tileset->compressed_size);
				ImGui::Text("Uncompressed Size: %02d (0x%04X)", tileset->uncompressed_size, tileset->uncompressed_size);
			}
		}
		ImGui::End();
	}

}