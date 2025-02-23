#include "ui/ui_tile_layout_viewer.h"

#include "ui/ui_editor.h"

#include "rom/rom_asset_definitions.h"
#include "rom/animated_object.h"
#include "rom/sprite.h"
#include "rom/tileset.h"
#include "rom/tile_layout.h"
#include "rom/game_objects/game_object_flipper.h"
#include "rom/game_objects/game_object_ring.h"
#include "rom/game_objects/game_object_definition.h"

#include "SDL3/SDL_image.h"
#include "imgui.h"
#include <memory>
#include <cassert>
#include <limits>
#include <algorithm>
#include <iterator>


namespace spintool
{
	void EditorTileLayoutViewer::Update()
	{
		if (IsOpen() == false)
		{
			return;
		}

		if (ImGui::Begin("Tile Layout Viewer", &m_visible, ImGuiWindowFlags_HorizontalScrollbar))
		{
			static std::vector<RenderTileLayoutRequest> s_tile_layout_render_requests;
			static std::vector<rom::FlipperInstance> s_flipper_instances;
			static std::vector<rom::RingInstance> s_ring_instances;
			static std::vector<rom::GameObjectDefinition> s_game_obj_instances;
			static std::vector<rom::AnimObjectDefinition> s_anim_obj_instances;

			struct AnimSpriteEntry
			{
				rom::Ptr32 sprite_table = 0;
				std::shared_ptr<const rom::AnimationSequence> anim_sequence;
				size_t anim_command_used_for_surface = 0;
				SDLSurfaceHandle sprite_surface;
			};
			static std::vector<AnimSpriteEntry> s_anim_sprite_instances;

			bool render_preview = false;
			static SDLSurfaceHandle LevelFlipperSpriteSurface;
			static SDLPaletteHandle LevelFlipperPalette;
			static SDLSurfaceHandle LevelRingSpriteSurface;
			static SDLPaletteHandle LevelRingPalette;
			static SDLSurfaceHandle LevelGameObjSpriteSurface;
			static SDLPaletteHandle LevelGameObjPalette;
			static rom::PaletteSet LevelPaletteSet;
			static bool export_result = false;

			static int level_index = 0;
			static rom::LevelDataOffsets level_data_offsets{ level_index };
			
			if (ImGui::SliderInt("###Level Index", &level_index, 0, 3))
			{
				level_data_offsets = rom::LevelDataOffsets{level_index};
			}

			bool render_both = ImGui::Button("Preview Level");
			ImGui::SameLine();
			bool render_bg = ImGui::Button("Preview level BG");
			ImGui::SameLine();
			bool render_fg = ImGui::Button("Preview level FG");
			ImGui::SameLine();
			ImGui::Checkbox("Export", &export_result);

			bool render_flippers = false;
			bool render_rings = false;

			if (render_both)
			{
				render_fg = true;
				render_bg = true;
				render_flippers = true;
				render_rings = true;
			}

			if (render_bg)
			{
				const auto& buffer = m_owning_ui.GetROM().m_buffer;
				const Uint32 BGTilesetOffsets = m_owning_ui.GetROM().ReadUint32(level_data_offsets.background_tileset);
				const Uint32 BGTilesetLayouts = m_owning_ui.GetROM().ReadUint32(level_data_offsets.background_tile_layout);
				const Uint32 BGTilesetBrushes = m_owning_ui.GetROM().ReadUint32(level_data_offsets.background_tile_brushes);

				LevelPaletteSet = *rom::PaletteSet::LoadFromROM(m_owning_ui.GetROM(), level_data_offsets.palette_set);

				const Uint16 LevelDimensionsX = m_owning_ui.GetROM().ReadUint16(level_data_offsets.tile_layout_width);
				const Uint16 LevelDimensionsY = m_owning_ui.GetROM().ReadUint16(level_data_offsets.tile_layout_height);

				RenderTileLayoutRequest request;

				request.tileset_address = BGTilesetOffsets;
				request.tile_brushes_address = BGTilesetBrushes;
				request.tile_brushes_address_end = BGTilesetLayouts;

				request.tile_brush_width = 4;
				request.tile_brush_height = 4;

				request.tile_layout_width = LevelDimensionsX / (rom::TileBrush<4, 4>::s_brush_width * rom::TileSet::s_tile_width);
				request.tile_layout_height = LevelDimensionsY / (rom::TileBrush<4, 4>::s_brush_height * rom::TileSet::s_tile_height);

				request.tile_layout_address = BGTilesetLayouts;
				request.tile_layout_address_end = request.tile_layout_address + (request.tile_layout_width * request.tile_layout_height * 2);

				request.is_chroma_keyed = false;
				request.compression_algorithm = CompressionAlgorithm::SSC;
				char levelname_buffer[32];
				sprintf_s(levelname_buffer, "level_%d", level_index);
				request.layout_type_name = levelname_buffer;
				request.layout_layout_name = "bg";

				s_tile_layout_render_requests.emplace_back(std::move(request));
			}

			if (render_fg)
			{
				const auto& buffer = m_owning_ui.GetROM().m_buffer;
				const rom::LevelDataOffsets level_data_offsets{ level_index };
				const Uint32 FGTilesetOffsets = m_owning_ui.GetROM().ReadUint32(level_data_offsets.foreground_tileset);
				const Uint32 FGTilesetLayouts = m_owning_ui.GetROM().ReadUint32(level_data_offsets.foreground_tile_layout);
				const Uint32 FGTilesetBrushes = m_owning_ui.GetROM().ReadUint32(level_data_offsets.foreground_tile_brushes);

				LevelPaletteSet = *rom::PaletteSet::LoadFromROM(m_owning_ui.GetROM(), level_data_offsets.palette_set);

				const Uint16 LevelDimensionsX = m_owning_ui.GetROM().ReadUint16(level_data_offsets.tile_layout_width);
				const Uint16 LevelDimensionsY = m_owning_ui.GetROM().ReadUint16(level_data_offsets.tile_layout_height);

				RenderTileLayoutRequest request;

				request.tileset_address = FGTilesetOffsets;
				request.tile_brushes_address = FGTilesetBrushes;
				request.tile_brushes_address_end = FGTilesetLayouts;

				request.tile_brush_width = 4;
				request.tile_brush_height = 4;

				request.tile_layout_width = LevelDimensionsX / (rom::TileBrush<4, 4>::s_brush_width * rom::TileSet::s_tile_width);
				request.tile_layout_height = LevelDimensionsY / (rom::TileBrush<4, 4>::s_brush_height * rom::TileSet::s_tile_height);

				request.tile_layout_address = FGTilesetLayouts;
				request.tile_layout_address_end = request.tile_layout_address + (request.tile_layout_width * request.tile_layout_height * 2);

				request.is_chroma_keyed = true;
				request.compression_algorithm = CompressionAlgorithm::SSC;

				char levelname_buffer[32];
				sprintf_s(levelname_buffer, "level_%d", level_index);
				request.layout_type_name = levelname_buffer;
				request.layout_layout_name = "fg";

				s_tile_layout_render_requests.emplace_back(std::move(request));
			}

			bool preview_intro_bg = ImGui::Button("Preview Intro Cutscene BG");
			ImGui::SameLine();
			bool preview_intro_fg = ImGui::Button("Preview Intro Cutscene FG");
			ImGui::SameLine();
			bool preview_intro_ship = ImGui::Button("Preview Intro Cutscene Robotnik Ship");
			ImGui::SameLine();
			bool preview_intro_water = ImGui::Button("Preview Intro Cutscene Water");
			
			bool preview_menu_bg = ImGui::Button("Preview Main Menu BG");
			ImGui::SameLine();
			bool preview_menu_fg = ImGui::Button("Preview Main Menu FG");
			ImGui::SameLine();
			bool preview_menu_combined = ImGui::Button("Preview Main Menu Combined");
			
			bool preview_bonus_bg = ImGui::Button("Preview Bonus BG");
			ImGui::SameLine();
			bool preview_bonus_fg = ImGui::Button("Preview Bonus FG");
			ImGui::SameLine();
			bool preview_bonus_combined = ImGui::Button("Preview Bonus Combined");
			ImGui::SameLine();
			static bool preview_bonus_alt_palette = false;
			ImGui::Checkbox("Alt palette", &preview_bonus_alt_palette);
			
			bool preview_sega_logo = ImGui::Button("Preview Sega Logo");
			ImGui::SameLine();
			bool preview_options = ImGui::Button("Preview Options Menu");

			if (preview_options)
			{
				RenderTileLayoutRequest request;

				request.tileset_address = rom::OptionsMenuTileset;
				request.tile_brushes_address = rom::OptionsMenuTileBrushes;
				request.tile_brushes_address_end = 0x000BE1BC;
				request.tile_layout_address = rom::OptionsMenuTileLayout;
				request.tile_layout_address_end = 0x000BE248;

				request.tile_brush_width = 4;
				request.tile_brush_height = 4;

				request.tile_layout_width = 0xA;
				request.tile_layout_height = 0x7;

				request.is_chroma_keyed = false;
				request.compression_algorithm = CompressionAlgorithm::SSC;

				LevelPaletteSet = *m_owning_ui.GetROM().GetOptionsScreenPaletteSet();

				s_tile_layout_render_requests.emplace_back(std::move(request));
			}


			if (preview_intro_bg)
			{
				{
					RenderTileLayoutRequest request;

					request.tileset_address = rom::IntroCutscenesTileset;
					request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(rom::IntroCutsceneTileLayoutSky);
					request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(rom::IntroCutsceneTileLayoutSky + sizeof(Uint16));
					request.tile_layout_address = rom::IntroCutsceneTileLayoutSky;
					request.palette_line = 1;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.is_chroma_keyed = false;
					request.show_brush_previews = false;
					request.compression_algorithm = CompressionAlgorithm::BOOGALOO;

					request.layout_type_name = "intro";
					request.layout_layout_name = "bg";

					LevelPaletteSet = *m_owning_ui.GetROM().GetIntroCutscenePaletteSet();
					s_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}

			if (preview_intro_fg)
			{
				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x000A3124 + 2;

					// Veg-o Fortress
					request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x000A1B8C);
					request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x000A1B8E);
					request.tile_layout_address = 0x000A1B8C;
					request.palette_line = 1;

					// Veg-o Fortress empty topsection
					//request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x000A2168);
					//request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x000A216A);
					//request.tile_layout_address = 0x000A2168;
					//request.palette_line = 1;


					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.is_chroma_keyed = false;
					request.show_brush_previews = false;
					request.compression_algorithm = CompressionAlgorithm::BOOGALOO;

					request.layout_type_name = "intro";
					request.layout_layout_name = "veg_o_fortress";

					LevelPaletteSet = *m_owning_ui.GetROM().GetIntroCutscenePaletteSet();

					s_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}

			if (preview_intro_ship)
			{
				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x000A3124 + 2;

					// Robotnik ship
					request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x000a30bc);
					request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x000a30be);
					request.tile_layout_address = 0x000a30bc;
					request.palette_line = 1;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.is_chroma_keyed = false;
					request.show_brush_previews = false;
					request.compression_algorithm = CompressionAlgorithm::BOOGALOO;

					request.layout_type_name = "intro";
					request.layout_layout_name = "robotnik_ship";

					LevelPaletteSet = *m_owning_ui.GetROM().GetIntroCutscenePaletteSet();

					s_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}

			if (preview_intro_water)
			{
				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x000A3124 + 2;

					// Water
					request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x000a220c);
					request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x000a220e);
					request.tile_layout_address = 0x000a220c;
					request.palette_line = 0;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.is_chroma_keyed = false;
					request.show_brush_previews = false;
					request.compression_algorithm = CompressionAlgorithm::BOOGALOO;

					request.layout_type_name = "intro";
					request.layout_layout_name = "water";

					LevelPaletteSet = *m_owning_ui.GetROM().GetIntroCutscenePaletteSet();

					s_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}

			if (preview_menu_bg || preview_menu_combined)
			{
				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x0009D102 + 2;
					request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x0009C82E);
					request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x0009C830);
					request.palette_line = 1;

					request.tile_layout_address = 0x0009C82E;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.is_chroma_keyed = false;
					request.show_brush_previews = false;
					request.compression_algorithm = CompressionAlgorithm::BOOGALOO;

					request.layout_type_name = "intro";
					request.layout_layout_name = "bg";

					LevelPaletteSet = *m_owning_ui.GetROM().GetMainMenuPaletteSet();
					s_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}

			if (preview_menu_fg || preview_menu_combined)
			{
				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x0009D102 + 2;
					request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x0009C05A);
					request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x0009C05C);
					request.tile_layout_address = 0x0009C05A;
					request.palette_line = 0;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.is_chroma_keyed = preview_menu_combined;
					request.show_brush_previews = false;
					request.compression_algorithm = CompressionAlgorithm::BOOGALOO;

					request.layout_type_name = "intro";
					request.layout_layout_name = "fg";

					LevelPaletteSet = *m_owning_ui.GetROM().GetMainMenuPaletteSet();
					s_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}

			if (preview_sega_logo)
			{
				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x000A3124 + 2;
					request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x000A14F8);
					request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x000A14FA);
					request.tile_layout_address = 0x000A14F8;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.is_chroma_keyed = false;
					request.show_brush_previews = false;
					request.compression_algorithm = CompressionAlgorithm::BOOGALOO;

					request.layout_type_name = "intro";
					request.layout_layout_name = "sega_logo";

					LevelPaletteSet = *m_owning_ui.GetROM().GetSegaLogoIntroPaletteSet();
					s_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}

			if (preview_bonus_bg || preview_bonus_combined)
			{
				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x000C77B0;
					request.tile_layout_address = 0x000C7350 - 4;
					request.tile_layout_address_end = 0x000c77b0 - 4;
					request.palette_line = preview_bonus_alt_palette ? 1 : 0;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.tile_layout_width = 0x14;
					request.tile_layout_height = static_cast<unsigned int>(((*request.tile_layout_address_end - request.tile_layout_address) / 2) / request.tile_layout_width);

					request.is_chroma_keyed = false;
					request.show_brush_previews = false;
					request.draw_mirrored_layout = true;
					request.compression_algorithm = CompressionAlgorithm::BOOGALOO;

					LevelPaletteSet = rom::PaletteSet{};
					LevelPaletteSet.palette_lines[0] = m_owning_ui.GetPalettes()[0x1f];
					LevelPaletteSet.palette_lines[1] = m_owning_ui.GetPalettes()[0x20];
					LevelPaletteSet.palette_lines[2] = m_owning_ui.GetPalettes()[0x21];
					LevelPaletteSet.palette_lines[3] = m_owning_ui.GetPalettes()[0x22];

					request.layout_type_name = "bonus";
					request.layout_layout_name = "bg";

					s_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}

			if (preview_bonus_fg || preview_bonus_combined)
			{
				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x000C77B0;
					request.tile_layout_address = 0x000C6EF0 - 4;
					request.tile_layout_address_end = 0x000C734D;
					request.palette_line = preview_bonus_alt_palette ? 0 : 1;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.tile_layout_width = 0x14;
					request.tile_layout_height = static_cast<unsigned int>(((*request.tile_layout_address_end - request.tile_layout_address) / 2) / request.tile_layout_width);

					request.is_chroma_keyed = preview_bonus_combined;
					request.show_brush_previews = false;
					request.draw_mirrored_layout = true;
					request.compression_algorithm = CompressionAlgorithm::BOOGALOO;

					LevelPaletteSet = rom::PaletteSet{};
					LevelPaletteSet.palette_lines[0] = m_owning_ui.GetPalettes()[0x1f];
					LevelPaletteSet.palette_lines[1] = m_owning_ui.GetPalettes()[0x20];
					LevelPaletteSet.palette_lines[2] = m_owning_ui.GetPalettes()[0x21];
					LevelPaletteSet.palette_lines[3] = m_owning_ui.GetPalettes()[0x22];

					request.layout_type_name = "bonus";
					request.layout_layout_name = "fg";

					s_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}

			if (render_flippers)
			{
				const static size_t flippers_ptr_table_offset = 0x000C08BE;
				const static size_t flippers_count_table_offset = 0x000C08EE;
				const static size_t flipper_palette[4] =
				{
					0x1,
					0x0,
					0x0,
					0x0
				};
				const static size_t flipper_sprite_offset[4] =
				{
					0x00017E82,
					0x0002594A,
					0x0002E05C,
					0x00035692
				};

				std::shared_ptr<const rom::Sprite> LevelFlipperSprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), flipper_sprite_offset[level_index]);
				LevelFlipperSpriteSurface = SDLSurfaceHandle{ SDL_CreateSurface(44, 31, SDL_PIXELFORMAT_INDEX8) };
				LevelFlipperPalette = Renderer::CreateSDLPalette(*LevelPaletteSet.palette_lines[flipper_palette[level_index]]);
				SDL_SetSurfacePalette(LevelFlipperSpriteSurface.get(), LevelFlipperPalette.get());
				SDL_ClearSurface(LevelFlipperSpriteSurface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
				SDL_SetSurfaceColorKey(LevelFlipperSpriteSurface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(LevelFlipperSpriteSurface->format), nullptr, 0, (Uint8)rom::Swatch::Pack(0, rom::Colour::levels_lookup[0x0], rom::Colour::levels_lookup[0x0]), 0, 0));
				LevelFlipperSprite->RenderToSurface(LevelFlipperSpriteSurface.get());

				const Uint32 flippers_table_begin = m_owning_ui.GetROM().ReadUint32(flippers_ptr_table_offset + (4 * level_index));
				const Uint32 num_flippers = m_owning_ui.GetROM().ReadUint16(flippers_count_table_offset + (2 * level_index));

				s_flipper_instances.clear();
				s_flipper_instances.reserve(num_flippers);

				size_t current_table_offset = flippers_table_begin;

				for (size_t i = 0; i < num_flippers; ++i)
				{
					s_flipper_instances.emplace_back(rom::FlipperInstance::LoadFromROM(m_owning_ui.GetROM(), current_table_offset));
					current_table_offset = s_flipper_instances.back().rom_data.rom_offset_end;
				}
			}

			if (render_rings)
			{
				const static size_t ring_sprite_offset = 0x0000F6D8;

				std::shared_ptr<const rom::Sprite> LevelRingSprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), ring_sprite_offset);
				LevelRingSpriteSurface = SDLSurfaceHandle{ SDL_CreateSurface(LevelRingSprite->GetBoundingBox().Width(), LevelRingSprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_INDEX8) };
				LevelRingPalette = Renderer::CreateSDLPalette(*LevelPaletteSet.palette_lines[3].get());
				SDL_SetSurfacePalette(LevelRingSpriteSurface.get(), LevelRingPalette.get());
				SDL_ClearSurface(LevelRingSpriteSurface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
				SDL_SetSurfaceColorKey(LevelRingSpriteSurface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(LevelRingSpriteSurface->format), nullptr, 0, 0, 0, 0));
				LevelRingSprite->RenderToSurface(LevelRingSpriteSurface.get());

				s_ring_instances.clear();
				s_ring_instances.reserve(level_data_offsets.ring_instances.count);

				{
					size_t current_table_offset = level_data_offsets.ring_instances.offset;

					for (size_t i = 0; i < level_data_offsets.ring_instances.count; ++i)
					{
						s_ring_instances.emplace_back(rom::RingInstance::LoadFromROM(m_owning_ui.GetROM(), current_table_offset));
						current_table_offset = s_ring_instances.back().rom_data.rom_offset_end;
					}
				}
				/////////////

				const static size_t game_obj_sprite_offset = 0x0000F6D8;

				std::shared_ptr<const rom::Sprite> LevelGameObjSprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), game_obj_sprite_offset);
				LevelGameObjSpriteSurface = SDLSurfaceHandle{ SDL_CreateSurface(LevelGameObjSprite->GetBoundingBox().Width(), LevelGameObjSprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_RGBA32) };
				LevelGameObjPalette = Renderer::CreateSDLPalette(*LevelPaletteSet.palette_lines[3].get());
				SDL_SetSurfacePalette(LevelGameObjSpriteSurface.get(), LevelGameObjPalette.get());
				SDL_ClearSurface(LevelGameObjSpriteSurface.get(), 255, 255, 255, 255);
				SDL_SetSurfaceColorKey(LevelGameObjSpriteSurface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(LevelGameObjSpriteSurface->format), nullptr, 255, 0, 0, 255));
				//LevelGameObjSprite->RenderToSurface(LevelGameObjSpriteSurface.get());

				s_game_obj_instances.clear();
				s_anim_sprite_instances.clear();
				s_game_obj_instances.reserve(level_data_offsets.object_instances.count);

				{
					size_t current_table_offset = level_data_offsets.object_instances.offset;

					for (size_t i = 0; i < level_data_offsets.object_instances.count; ++i)
					{
						s_game_obj_instances.emplace_back(rom::GameObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), current_table_offset));
						const rom::Ptr32 anim_def_offset = s_game_obj_instances.back().animation_definition;
						rom::AnimObjectDefinition anim_obj = rom::AnimObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), anim_def_offset);

						auto found_sprite = std::find_if(std::begin(s_anim_sprite_instances), std::end(s_anim_sprite_instances), [&anim_obj](const AnimSpriteEntry& entry)
							{
								return anim_obj.sprite_table == entry.sprite_table && anim_obj.starting_animation == entry.anim_sequence->rom_data.rom_offset;
							});

						if (found_sprite == std::end(s_anim_sprite_instances))
						{
							AnimSpriteEntry entry;
							entry.sprite_table = m_owning_ui.GetROM().ReadUint32(anim_obj.sprite_table);
							entry.anim_sequence = rom::AnimationSequence::LoadFromROM(m_owning_ui.GetROM(), anim_obj.starting_animation, entry.sprite_table);

							const auto& command_sequence = entry.anim_sequence->command_sequence;
							const auto& first_frame_with_sprite = std::find_if(std::begin(command_sequence), std::end(command_sequence), [](const rom::AnimationCommand& command)
								{
									return command.ui_frame_sprite != nullptr;
								});

							if (first_frame_with_sprite != std::end(command_sequence))
							{
								entry.anim_command_used_for_surface = std::distance(std::begin(command_sequence), first_frame_with_sprite);
								const auto& first_frame_sprite = first_frame_with_sprite->ui_frame_sprite->sprite;
								auto new_sprite_surface = SDLSurfaceHandle{ SDL_CreateSurface(first_frame_sprite->GetBoundingBox().Width(), first_frame_sprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_INDEX8) };
								LevelRingPalette = Renderer::CreateSDLPalette(*LevelPaletteSet.palette_lines[3].get());
								SDL_SetSurfacePalette(new_sprite_surface.get(), LevelRingPalette.get());
								SDL_ClearSurface(new_sprite_surface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
								SDL_SetSurfaceColorKey(new_sprite_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(new_sprite_surface->format), nullptr, 0, 0, 0, 0));
								first_frame_sprite->RenderToSurface(new_sprite_surface.get());

								entry.sprite_surface = std::move(new_sprite_surface);
							}


							s_anim_sprite_instances.emplace_back(std::move(entry));
						}
						current_table_offset = s_game_obj_instances.back().rom_data.rom_offset_end;
					}
				}
			}

			const bool will_be_rendering_preview = s_tile_layout_render_requests.empty() == false;
			const bool export_combined = s_tile_layout_render_requests.size() > 1;
			static std::optional<RenderTileLayoutRequest> current_preview_data;
			if (will_be_rendering_preview && export_combined == false)
			{
				current_preview_data = s_tile_layout_render_requests.front();
			}

			char combined_buffer[128];
			if (export_combined)
			{
				sprintf_s(combined_buffer, "%s_combined", s_tile_layout_render_requests.front().layout_type_name.c_str());
			}
			const std::string combined_layout_name = export_combined ? s_tile_layout_render_requests.front().layout_layout_name : "";
			const std::string combined_type_name = export_combined ? combined_buffer : "";
			SDLSurfaceHandle layout_preview_surface;
			if (will_be_rendering_preview)
			{
				bool will_require_mirror = false;
				int largest_width = std::numeric_limits<int>::min();
				int largest_height = std::numeric_limits<int>::min();

				for (const auto& request : s_tile_layout_render_requests)
				{
					largest_width = std::max<int>(request.tile_layout_width * request.tile_brush_width, largest_width);
					largest_height = std::max<int>(request.tile_layout_height * request.tile_brush_height, largest_height);
					will_require_mirror |= request.draw_mirrored_layout;
				}

				if (will_require_mirror)
				{
					largest_width *= 2;
				}

				const RenderTileLayoutRequest& request = s_tile_layout_render_requests.front();
				layout_preview_surface = SDLSurfaceHandle{ SDL_CreateSurface(rom::TileSet::s_tile_width * largest_width, rom::TileSet::s_tile_height * largest_height, SDL_PIXELFORMAT_RGBA32) };
				SDL_ClearSurface(layout_preview_surface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
			}

			while (s_tile_layout_render_requests.empty() == false)
			{
				const RenderTileLayoutRequest& request = s_tile_layout_render_requests.front();
				if (request.compression_algorithm == CompressionAlgorithm::SSC)
				{
					m_tileset = rom::TileSet::LoadFromROM(m_owning_ui.GetROM(), request.tileset_address).tileset;
					m_tile_layout = rom::TileLayout::LoadFromROM(m_owning_ui.GetROM(), request.tile_brushes_address, request.tile_brushes_address_end, request.tile_layout_address, request.tile_layout_address_end);
				}
				else if (request.compression_algorithm == CompressionAlgorithm::BOOGALOO)
				{
					m_tileset = rom::TileSet::LoadFromROMSecondCompression(m_owning_ui.GetROM(), request.tileset_address).tileset;
					m_tile_layout = rom::TileLayout::LoadFromROM(m_owning_ui.GetROM(), *m_tileset, request.tile_layout_address, request.tile_layout_address_end);
				}

				if (export_combined == false)
				{
					current_preview_data->tile_layout_address = m_tile_layout->rom_data.rom_offset;
					current_preview_data->tile_layout_address_end = m_tile_layout->rom_data.rom_offset_end;
				}
				std::shared_ptr<const rom::Sprite> tileset_sprite = m_tileset->CreateSpriteFromTile(0);
				std::shared_ptr<const rom::Sprite> tile_layout_sprite = std::make_unique<rom::Sprite>();

				std::vector<rom::Sprite> brushes;
				brushes.reserve(m_tile_layout->tile_brushes.size());
				m_tile_brushes_previews.reserve(brushes.size());
				m_tile_brushes_previews.clear();

				const int brush_width = static_cast<int>(request.tile_brush_width * rom::TileSet::s_tile_width);
				const int brush_height = static_cast<int>(request.tile_brush_height * rom::TileSet::s_tile_height);

				for (size_t brush_index = 0; brush_index < m_tile_layout->tile_brushes.size(); ++brush_index)
				{
					rom::TileBrushBase& brush = *m_tile_layout->tile_brushes[brush_index].get();
					rom::Sprite& brush_sprite = brushes.emplace_back();
					for (size_t i = 0; i < brush.tiles.size(); ++i)
					{
						rom::TileInstance& tile = brush.tiles[i];
						std::shared_ptr<rom::SpriteTile> sprite_tile = m_tileset->CreateSpriteTileFromTile(tile.tile_index);

						if (sprite_tile == nullptr)
						{
							break;
						}

						const size_t current_brush_offset = i;

						sprite_tile->x_offset = static_cast<Sint16>(current_brush_offset % brush.BrushWidth()) * rom::TileSet::s_tile_width;
						sprite_tile->y_offset = static_cast<Sint16>((current_brush_offset - (current_brush_offset % brush.BrushWidth())) / brush.BrushWidth()) * rom::TileSet::s_tile_height;

						sprite_tile->blit_settings.flip_horizontal = tile.is_flipped_horizontally;
						sprite_tile->blit_settings.flip_vertical = tile.is_flipped_vertically;

						sprite_tile->blit_settings.palette = tile.palette_line == 0 && request.palette_line.has_value() ? LevelPaletteSet.palette_lines.at(*request.palette_line) : LevelPaletteSet.palette_lines.at(tile.palette_line);
						brush_sprite.sprite_tiles.emplace_back(std::move(sprite_tile));
					}
					brush_sprite.num_tiles = static_cast<Uint16>(brush_sprite.sprite_tiles.size());
					SDLSurfaceHandle new_surface{ SDL_CreateSurface(brush_sprite.GetBoundingBox().Width(), brush_sprite.GetBoundingBox().Height(), SDL_PIXELFORMAT_RGBA32) };
					SDL_SetSurfaceColorKey(new_surface.get(), request.is_chroma_keyed, SDL_MapRGBA(SDL_GetPixelFormatDetails(new_surface->format), nullptr, 0, 0, 0, 0));
					SDL_ClearSurface(new_surface.get(), 0.0f, 0, 0, 0);
					brush_sprite.RenderToSurface(new_surface.get());
					SDL_Surface* the_surface = new_surface.get();
					m_tile_brushes_previews.emplace_back(TileBrushPreview{ std::move(new_surface), Renderer::RenderToTexture(the_surface) });
				}

				for (size_t i = 0; i < m_tile_layout->tile_brush_instances.size(); ++i)
				{
					const auto tile_brush_index = m_tile_layout->tile_brush_instances[i].tile_brush_index;
					if (tile_brush_index >= m_tile_brushes_previews.size())
					{
						break;
					}

					SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(m_tile_brushes_previews[tile_brush_index].surface.get()) };
					if (m_tile_layout->tile_brush_instances[i].is_flipped_horizontally)
					{
						SDL_FlipSurface(temp_surface.get(), SDL_FLIP_HORIZONTAL);
					}

					if (m_tile_layout->tile_brush_instances[i].is_flipped_vertically)
					{
						SDL_FlipSurface(temp_surface.get(), SDL_FLIP_VERTICAL);
					}

					const int x_off = static_cast<int>((i % request.tile_layout_width) * brush_width);
					const int y_off = static_cast<int>(((i - (i % request.tile_layout_width)) / request.tile_layout_width) * brush_height);

					const SDL_Rect target_rect{ x_off, y_off, brush_width, brush_height };
					SDL_SetSurfaceColorKey(temp_surface.get(), request.is_chroma_keyed, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_surface.get(), &target_rect);
				}

				if (request.draw_mirrored_layout)
				{
					for (size_t i = 0; i < m_tile_layout->tile_brush_instances.size(); ++i)
					{
						const auto tile_brush_index = m_tile_layout->tile_brush_instances[i].tile_brush_index;
						if (tile_brush_index >= m_tile_brushes_previews.size())
						{
							break;
						}

						SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(m_tile_brushes_previews[tile_brush_index].surface.get()) };
						if (m_tile_layout->tile_brush_instances[i].is_flipped_horizontally == false)
						{
							SDL_FlipSurface(temp_surface.get(), SDL_FLIP_HORIZONTAL);
						}

						if (m_tile_layout->tile_brush_instances[i].is_flipped_vertically)
						{
							SDL_FlipSurface(temp_surface.get(), SDL_FLIP_VERTICAL);
						}

						const int x_off = ((request.tile_layout_width * 2 * brush_width)- brush_width) - static_cast<int>((i % request.tile_layout_width) * brush_width);
						const int y_off = static_cast<int>(((i - (i % request.tile_layout_width)) / request.tile_layout_width) * brush_height);

						const SDL_Rect target_rect{ x_off, y_off, brush_width, brush_height };
						SDL_SetSurfaceColorKey(temp_surface.get(), request.is_chroma_keyed, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
						SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_surface.get(), &target_rect);
					}
				}

				if (request.show_brush_previews == false)
				{
					m_tile_brushes_previews.clear();
				}

				if (export_result && export_combined == false)
				{
					static char path_buffer[4096];
					sprintf_s(path_buffer, "spinball_%s_%s.png", request.layout_type_name.c_str(), request.layout_layout_name.c_str());
					std::filesystem::path export_path = m_owning_ui.GetSpriteExportPath().append(path_buffer);
					assert(IMG_SavePNG(layout_preview_surface.get(), export_path.generic_u8string().c_str()));
				}

				s_tile_layout_render_requests.erase(std::begin(s_tile_layout_render_requests));
			}

			if (render_flippers)
			{
				for (size_t i = 0; i < s_flipper_instances.size(); ++i)
				{
					const rom::FlipperInstance& flipper = s_flipper_instances[i];

					SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(LevelFlipperSpriteSurface.get()) };
					int x_off = -24;
					if (flipper.is_x_flipped)
					{
						SDL_FlipSurface(temp_surface.get(), SDL_FLIP_HORIZONTAL);
						x_off = -20;
					}

					SDL_Rect target_rect{ flipper.x_pos + x_off, flipper.y_pos - 31, 44, 31 };
					SDL_SetSurfaceColorKey(temp_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_surface.get(), &target_rect);
				}
			}

			if (render_rings)
			{
				for (size_t i = 0; i < s_ring_instances.size(); ++i)
				{
					const rom::RingInstance& ring = s_ring_instances[i];

					SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(LevelRingSpriteSurface.get()) };
					SDL_Rect target_rect{ ring.x_pos - 8, ring.y_pos - 16, 0x10, 0x10 };
					SDL_SetSurfaceColorKey(temp_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_surface.get(), &target_rect);
				}

				
				static rom::Colour bbox_colours[]
				{
					{255,255,255,255},
					{255,255,0,0},
					{255,0,255,0},
					{255,16,0,255},
					{255,255,255,0},
					{255,255,0,255},
					{255,0,255,255},
					{255,128,128,128},
					{255,64,192,128},
					{255,192,32,210},
					{255,128,128,255},
					{255,128,128,255},
				};


				for (size_t i = 0; i < s_game_obj_instances.size(); ++i)
				{
					const rom::GameObjectDefinition& game_obj = s_game_obj_instances[i];


					rom::AnimObjectDefinition anim_obj = rom::AnimObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), s_game_obj_instances[i].animation_definition);

					auto anim_entry = std::find_if(std::begin(s_anim_sprite_instances), std::end(s_anim_sprite_instances), [&anim_obj](const AnimSpriteEntry& entry)
						{
							return entry.anim_sequence->rom_data.rom_offset == anim_obj.starting_animation;
						});

					if (anim_entry != std::end(s_anim_sprite_instances) && anim_entry->sprite_surface != nullptr)
					{
						const rom::AnimationCommand& command_sprite_came_from = anim_entry->anim_sequence->command_sequence.at(anim_entry->anim_command_used_for_surface);
						auto origin_offset = command_sprite_came_from.ui_frame_sprite->sprite->GetOriginOffsetFromMinBounds();

						SDLSurfaceHandle temp_sprite_surface{ SDL_ScaleSurface(anim_entry->sprite_surface.get(), static_cast<int>(command_sprite_came_from.ui_frame_sprite->dimensions.x), static_cast<int>(command_sprite_came_from.ui_frame_sprite->dimensions.y), SDL_SCALEMODE_NEAREST) };
						SDL_Rect sprite_target_rect{ game_obj.x_pos - origin_offset.x, game_obj.y_pos - origin_offset.y, anim_entry->sprite_surface->w, anim_entry->sprite_surface->h };

						if (game_obj.flip_x)
						{
							SDL_FlipSurface(temp_sprite_surface.get(), SDL_FLIP_HORIZONTAL);
							//sprite_target_rect.x = game_obj.x_pos + (anim_entry->sprite_surface->w - origin_offset.x);
						}

						if (game_obj.flip_y)
						{
							SDL_FlipSurface(temp_sprite_surface.get(), SDL_FLIP_VERTICAL);
							//sprite_target_rect.y = game_obj.y_pos + (anim_entry->sprite_surface->h - origin_offset.y);
						}

						SDL_SetSurfaceColorKey(temp_sprite_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_sprite_surface->format), nullptr, 0, 0, 0, 255));
						SDL_BlitSurfaceScaled(temp_sprite_surface.get(), nullptr, layout_preview_surface.get(), &sprite_target_rect, SDL_SCALEMODE_NEAREST);
					}
					else
					{
						SDLSurfaceHandle temp_surface{ SDL_ScaleSurface(LevelGameObjSpriteSurface.get(), game_obj.collision_width, game_obj.collision_height, SDL_SCALEMODE_NEAREST) };

						auto cutting_surface = SDLSurfaceHandle{ SDL_CreateSurface(temp_surface->w, temp_surface->h, SDL_PIXELFORMAT_RGBA32) };
						SDL_ClearSurface(temp_surface.get(), bbox_colours[game_obj.type_id % std::size(bbox_colours)].r / 256.0f, bbox_colours[game_obj.type_id % std::size(bbox_colours)].g / 256.0f, bbox_colours[game_obj.type_id % std::size(bbox_colours)].b / 256.0f, 255.0f);
						SDL_ClearSurface(cutting_surface.get(), 255.0f, 0.0f, 0.0f, 255.0f);
						const SDL_Rect cutting_target_rect{ 2,2,temp_surface->w - 4, temp_surface->h - 4 };
						SDL_BlitSurfaceScaled(cutting_surface.get(), nullptr, temp_surface.get(), &cutting_target_rect, SDL_SCALEMODE_NEAREST);

						SDL_Rect target_rect{ game_obj.x_pos - game_obj.collision_width / 2, game_obj.y_pos - game_obj.collision_height, game_obj.collision_width, game_obj.collision_height };
						SDL_SetSurfaceColorKey(temp_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 255, 0, 0, 255));
						SDL_BlitSurfaceScaled(temp_surface.get(), nullptr, layout_preview_surface.get(), &target_rect, SDL_SCALEMODE_NEAREST);
					}
				}
			}

			if (export_result && export_combined)
			{
				static char path_buffer[4096];

				sprintf_s(path_buffer, "spinball_%s.png", combined_type_name.c_str());
				std::filesystem::path export_path = m_owning_ui.GetSpriteExportPath().append(path_buffer);
				assert(IMG_SavePNG(layout_preview_surface.get(), export_path.generic_u8string().c_str()));
			}

			if (will_be_rendering_preview)
			{
				m_tile_layout_preview = Renderer::RenderToTexture(layout_preview_surface.get());
			}

			constexpr size_t preview_brushes_per_row = 32;
			constexpr const float zoom = 1.0f;
			if (current_preview_data && ImGui::TreeNode("ROM Info"))
			{
				const auto address_end = current_preview_data->tile_layout_address_end.value_or(current_preview_data->tile_layout_address + (current_preview_data->tile_layout_width * 2) * current_preview_data->tile_layout_height);
				ImGui::Text("Tileset Compressed Data: 0x%08X", current_preview_data->tileset_address);
				ImGui::Text("Tileset Brushes: 0x%08X => 0x%08X", current_preview_data->tile_brushes_address, current_preview_data->tile_brushes_address_end);
				ImGui::Text("Tile Layout: 0x%08X => 0x%08X", current_preview_data->tile_layout_address, address_end);
				ImGui::Text("Layout size: %d", address_end - current_preview_data->tile_layout_address);
				ImGui::Text("Width: %d", current_preview_data->tile_layout_width);
				ImGui::Text("Height: %d", current_preview_data->tile_layout_height);
				ImGui::Text("Num tiles: %d", current_preview_data->tile_layout_width * current_preview_data->tile_layout_height);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Palette Set"))
			{
				for (const std::shared_ptr<rom::Palette>& palette : LevelPaletteSet.palette_lines)
				{
					DrawPaletteSwatchPreview(*palette);
				}
				ImGui::TreePop();
			}

			if (m_tile_brushes_previews.empty() == false && ImGui::TreeNode("Brush Previews"))
			{
				size_t i = 0;
				for (const TileBrushPreview& preview_brush : m_tile_brushes_previews)
				{
					if (preview_brush.texture != nullptr)
					{
						if (i % preview_brushes_per_row != 0)
						{
							ImGui::SameLine();
						}

						ImGui::Image((ImTextureID)preview_brush.texture.get(), ImVec2(static_cast<float>(preview_brush.texture->w) * zoom, static_cast<float>(preview_brush.texture->h) * zoom));
						if (ImGui::BeginItemTooltip())
						{
							ImGui::Text("Tile Index: 0x%02X", i);
							ImGui::EndTooltip();
						}
						++i;
					}
				}
				ImGui::TreePop();
			}
			if (m_tile_layout_preview != nullptr)
			{
				ImGui::Image((ImTextureID)m_tile_layout_preview.get(), ImVec2(static_cast<float>(m_tile_layout_preview->w) * zoom, static_cast<float>(m_tile_layout_preview->h) * zoom));
			}
		}
		ImGui::End();
	}
}