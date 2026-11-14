#include "ui/ui_tileset_navigator.h"

#include "ui/ui_editor.h"
#include "rom/spinball_rom.h"
#include "rom/ssc_decompressor.h"
#include "rom/rom_address_map.h"
#include <iostream>

namespace spintool
{
	rom::UsaAddress s_tile_offsets[] =
	{
		{ rom::AddressGroup::Common, 0x0003DBB2 }, // Toxic Caves
		{ rom::AddressGroup::Common, 0x000394AA },
		{ rom::AddressGroup::Common, 0x00067672 }, // Lava powerhouse
		{ rom::AddressGroup::Common, 0x00064BB6 },
		{ rom::AddressGroup::Common, 0x00081EB6 }, // The Machine
		{ rom::AddressGroup::Common, 0x0007F29C },
		{ rom::AddressGroup::Common, 0x00053214 }, // Showdown
		{ rom::AddressGroup::Common, 0x0004FEE4 },
		{ rom::AddressGroup::Frontend, 0x000BDD2E }, // Options
	};

	rom::UsaAddress s_tile_offsets_non_ssc[] =
	{
		{ rom::AddressGroup::Frontend, 0x0009D104 }, // Main Menu BG tiles
		{ rom::AddressGroup::Frontend, 0x000A3126 }, // Veg-o-fortress BG tiles
		{ rom::AddressGroup::Frontend, 0x000A220C }, // Veg-o-fortress FG tiles
		{ rom::AddressGroup::BonusStage, 0x000C77B0 }, // Bonus Stage BG tiles
		{ rom::AddressGroup::BonusStage, 0x000C9016 }, // Bonus stage FG tiles
		{ rom::AddressGroup::BonusStage, 0x000CB14E }, // Bonus stage FG tiles
		{ rom::AddressGroup::BonusStage, 0x000D10B0 }, // Bonus stage FG tiles
		{ rom::AddressGroup::BonusStage, 0x000CC710 }, // Bonus stage FG tiles
		{ rom::AddressGroup::BonusStage, 0x000CF2DE } // Bonus stage FG tiles
	};

	void EditorTilesetNavigator::Update()
	{
		if (m_visible == false)
		{
			return;
		}

		if (ImGui::Begin("Tileset Navigator", &m_visible))
		{
			if (ImGui::Button("Load all tilesets"))
			{
				for (const rom::UsaAddress& offset : s_tile_offsets)
				{
					m_tilesets.emplace_back(rom::TileSet::LoadFromROM_SSCCompression(m_owning_ui.GetROM(), offset.Get()));
				}

				for (const rom::UsaAddress& offset : s_tile_offsets_non_ssc)
				{
					m_tilesets.emplace_back(rom::TileSet::LoadFromROM_LZSSCompression(m_owning_ui.GetROM(), offset.Get()));
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
				const auto& rom_buffer = m_owning_ui.GetROM().m_buffer;
				if (actual_offset < 0 || static_cast<size_t>(actual_offset) >= rom_buffer.size())
				{
					result = {};
					result.error_msg = "ROM offset is outside the loaded ROM";
				}
				else
				{
					result = rom::SSCDecompressor::IsValidSSCCompressedData(rom_buffer.data() + actual_offset, static_cast<Uint32>(actual_offset));
				}
			}
			if (validated_data)
			{
				ImGui::Text(result.error_msg.value_or(std::string("OK!")).c_str());
			}

			static std::vector<rom::SSCDecompressionResult> valid_ssc_results;
			valid_ssc_results.reserve(0xFFFF);
			if (ImGui::Button("Search for SSC Compressed data"))
			{
				constexpr Uint32 max_tiles = 256;
				valid_ssc_results.clear();
				Uint32 next_offset = actual_offset;
				while (next_offset+2 < m_owning_ui.GetROM().m_buffer.size())
				{
					Uint16 num_tiles = (static_cast<Sint16>(m_owning_ui.GetROM().m_buffer[next_offset] << 8)) | (static_cast<Sint16>(m_owning_ui.GetROM().m_buffer[next_offset + 1]));
					if (num_tiles >= 2 && num_tiles < max_tiles)
					{
						rom::SSCDecompressionResult result = rom::SSCDecompressor::IsValidSSCCompressedData(&m_owning_ui.GetROM().m_buffer[next_offset+2], next_offset+2);
						//rom::SSCDecompressionResult decompressed_data_result = rom::SSCDecompressor::DecompressData(&m_owning_ui.GetROM().m_buffer[next_offset+2], num_tiles * 0x40);
						next_offset += 2;// result.rom_data.rom_offset_end;
						if (result.error_msg.has_value() == false)
						{
							//if (result.uncompressed_size != decompressed_data_result.uncompressed_data.size())
							//{
							//	//char buffer[2048];
							//	//sprintf(buffer, "Mismatched size detected at 0x%08X [Expected: %llu] [Actual: %llu]", static_cast<unsigned int>(result.rom_data.rom_offset), result.uncompressed_size, decompressed_data_result.uncompressed_data.size());
							//	//std::cout << buffer << std::endl;
							//}

							if (result.uncompressed_size > 0x400 && (valid_ssc_results.empty() || result.rom_data.rom_offset_end != valid_ssc_results.back().rom_data.rom_offset_end))
							{
								//std::shared_ptr<const rom::TileSet> tileset_result = m_owning_ui.GetROM().LoadTileData(next_offset);
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

			for (const TilesetEntry& tileset_entry : m_tilesets)
			{
				const std::unique_ptr<rom::TileSet>& tileset = tileset_entry.tileset;

				static std::string s_no_errors_str = "No Errors";
				if (!tileset)
				{
					ImGui::TextUnformatted("Invalid tileset entry");
					continue;
				}

				char name_buffer[1024];
				const Uint32 display_end = tileset->rom_data.rom_offset_end > 0 ? tileset->rom_data.rom_offset_end - 1 : 0;
				snprintf(name_buffer, sizeof(name_buffer), "Tileset (0x%06X -> 0x%06X) [%s]", static_cast<unsigned int>(tileset->rom_data.rom_offset), static_cast<unsigned int>(display_end), tileset_entry.result.error_msg.value_or(s_no_errors_str).c_str());
				if (ImGui::TreeNode(name_buffer))
				{
					ImGui::Text("Total size on ROM: %02d (0x%04X)", tileset->rom_data.real_size, tileset->rom_data.real_size);
					sprintf(name_buffer,"Header (0x%06X -> 0x%06X)", static_cast<unsigned int>(tileset->rom_data.rom_offset), static_cast<unsigned int>(tileset->rom_data.rom_offset + 2));
					if (ImGui::TreeNodeEx(name_buffer, ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Text("Tiles: %02d (0x%04X)", tileset->num_tiles, tileset->num_tiles);
						ImGui::TreePop();
					}

					sprintf(name_buffer, "Tile Data: (0x%06X -> 0x%06X)", static_cast<unsigned int>(tileset->rom_data.rom_offset + 2), static_cast<unsigned int>(tileset->rom_data.rom_offset_end > 0 ? tileset->rom_data.rom_offset_end - 1 : 0));
					if (ImGui::TreeNodeEx(name_buffer, ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Text("Compressed Size: %02d (0x%04X)", tileset->compressed_size, tileset->compressed_size);
						ImGui::Text("Uncompressed Size: %02d (0x%04X)", tileset->uncompressed_size, tileset->uncompressed_size);
						if (tileset->uncompressed_size > 0)
					{
						ImGui::Text("Compression Ratio: %.2f%%", (tileset->compressed_size / static_cast<float>(tileset->uncompressed_size)) * 100.0f);
					}
					else
					{
						ImGui::TextUnformatted("Compression Ratio: unavailable");
					}
						ImGui::TreePop();
					}
					ImGui::TreePop();
				}
			}
		}
		ImGui::End();
	}

}
