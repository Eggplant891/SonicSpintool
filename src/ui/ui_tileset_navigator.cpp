#include "ui/ui_tileset_navigator.h"

#include "ui/ui_editor.h"
#include "rom/spinball_rom.h"
#include "rom/ssc_decompressor.h"
#include <iostream>

namespace spintool
{
	EditorTilesetNavigator::EditorTilesetNavigator(EditorUI& owning_ui)
		: m_owning_ui(owning_ui)
		, m_rom(owning_ui.GetROM())
	{

	}

	size_t s_tile_offsets[] =
	{
		0x0003DBB2, // Toxic Caves
		0x000394AA,
		0x00067672, // Lava powerhouse
		0x00064BB6,
		0x00081EB6, // The Machine
		0x0007F29C,
		0x00053214, // Showdown
		0x0004FEE4,
		0x000BDD2E, // Options
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
					m_tilesets.emplace_back(rom::TileSet::LoadFromROM(m_owning_ui.GetROM(), offset));
				}
			}

			static int actual_offset = 0;
			static bool validated_data = false;
			static rom::SSCDecompressionResult result;
			if (ImGui::InputInt("ROM Offset", &actual_offset, 1, 0x10, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				validated_data = false;
			}

			if (ImGui::Button("Validate SSC Data"))
			{
				validated_data = true;
				result = rom::SSCDecompressor::IsValidSSCCompressedData(&m_rom.m_buffer[actual_offset], actual_offset);
			}
			if (validated_data)
			{
				ImGui::Text(result.error_msg.value_or(std::string("OK!")).c_str());
			}

			static std::vector<rom::SSCDecompressionResult> valid_ssc_results;
			valid_ssc_results.reserve(0xFFFF);
			if (ImGui::Button("Search for SSC Compressed data"))
			{
				constexpr size_t max_tiles = 256;
				valid_ssc_results.clear();
				size_t next_offset = actual_offset;
				while (next_offset+2 < m_rom.m_buffer.size())
				{
					Uint16 num_tiles = (static_cast<Sint16>(m_rom.m_buffer[next_offset] << 8)) | (static_cast<Sint16>(m_rom.m_buffer[next_offset + 1]));
					if (num_tiles >= 2 && num_tiles < max_tiles)
					{
						rom::SSCDecompressionResult result = rom::SSCDecompressor::IsValidSSCCompressedData(&m_rom.m_buffer[next_offset+2], next_offset+2);
						//rom::SSCDecompressionResult decompressed_data_result = rom::SSCDecompressor::DecompressData(&m_rom.m_buffer[next_offset+2], num_tiles * 0x40);
						next_offset += 2;// result.rom_data.rom_offset_end;
						if (result.error_msg.has_value() == false)
						{
							//if (result.uncompressed_size != decompressed_data_result.uncompressed_data.size())
							//{
							//	//char buffer[2048];
							//	//sprintf_s(buffer, "Mismatched size detected at 0x%08X [Expected: %llu] [Actual: %llu]", static_cast<unsigned int>(result.rom_data.rom_offset), result.uncompressed_size, decompressed_data_result.uncompressed_data.size());
							//	//std::cout << buffer << std::endl;
							//}

							if (result.uncompressed_size > 0x400 && (valid_ssc_results.empty() || result.rom_data.rom_offset_end != valid_ssc_results.back().rom_data.rom_offset_end))
							{
								//std::shared_ptr<const rom::TileSet> tileset_result = m_rom.LoadTileData(next_offset);
								//if (tileset_result->num_tiles == num_tiles)
								{
									valid_ssc_results.emplace_back(result);
									//valid_ssc_results.emplace_back(result, std::move(tileset_result));
								}
							}
						}

						if (next_offset % 2 == 1)
						{
							next_offset++;
						}
					}
					else
					{
						next_offset += 2;
					}
				}
			}

			if (valid_ssc_results.empty() == false)
			{
				ImGui::SeparatorText("Valid SSC data found");
			}
			for (const rom::SSCDecompressionResult& result : valid_ssc_results)
			{
				ImGui::Text("0x%08X -> 0x%08X [0x%02X] (Tiles: %u)", result.rom_data.rom_offset, result.rom_data.rom_offset_end, result.uncompressed_size);
			}

			for (const std::shared_ptr<const rom::TileSet>& tileset : m_tilesets)
			{
				char name_buffer[1024];
				sprintf_s(name_buffer, "Tileset (0x%06X -> 0x%06X)", static_cast<unsigned int>(tileset->rom_data.rom_offset), static_cast<unsigned int>(tileset->rom_data.rom_offset_end-1));
				if (ImGui::TreeNode(name_buffer))
				{
					ImGui::Text("Total size on ROM: %02d (0x%04X)", tileset->rom_data.real_size, tileset->rom_data.real_size);
					sprintf_s(name_buffer,"Header (0x%06X -> 0x%06X)", static_cast<unsigned int>(tileset->rom_data.rom_offset), static_cast<unsigned int>(tileset->rom_data.rom_offset + 2));
					if (ImGui::TreeNodeEx(name_buffer, ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Text("Tiles: %02d (0x%04X)", tileset->num_tiles, tileset->num_tiles);
						ImGui::TreePop();
					}

					sprintf_s(name_buffer, "Tile Data: (0x%06X -> 0x%06X)", static_cast<unsigned int>(tileset->rom_data.rom_offset + 2), static_cast<unsigned int>(tileset->rom_data.rom_offset_end-1));
					if (ImGui::TreeNodeEx(name_buffer, ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Text("Compressed Size: %02d (0x%04X)", tileset->compressed_size, tileset->compressed_size);
						ImGui::Text("Uncompressed Size: %02d (0x%04X)", tileset->uncompressed_size, tileset->uncompressed_size);
						ImGui::TreePop();
					}
					ImGui::TreePop();
				}
			}
		}
		ImGui::End();
	}

}