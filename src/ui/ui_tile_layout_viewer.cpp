#include "ui/ui_tile_layout_viewer.h"

#include "ui/ui_editor.h"

#include "rom/rom_asset_definitions.h"
#include "rom/animated_object.h"
#include "rom/sprite.h"
#include "rom/tileset.h"
#include "rom/tile_layout.h"

#include "SDL3/SDL_image.h"
#include "imgui.h"
#include <memory>
#include <cassert>
#include <limits>
#include <algorithm>
#include <iterator>


namespace spintool
{
	struct SpriteObjectPreview
	{
		SDLSurfaceHandle sprite;
		SDLPaletteHandle palette;
	};

	struct LayerSettings
	{
		bool background = true;
		bool foreground = true;
		bool foreground_high_priority = true;
		bool rings = true;
		bool flippers = true;
		bool invisible_objects = true;
		bool visible_objects = true;
		bool collision = true;
	};

	void EditorTileLayoutViewer::Update()
	{
		if (IsOpen() == false)
		{
			return;
		}
		ImGui::SetNextWindowPos(ImVec2{ 0,16 });
		ImGui::SetNextWindowSize(ImVec2{ Renderer::s_window_width, Renderer::s_window_height - 16 });
		if (ImGui::Begin("Tile Layout Viewer", &m_visible, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoSavedSettings))
		{
			bool render_preview = false;
			static LayerSettings layer_settings;
			static SpriteObjectPreview FlipperPreview;
			static SpriteObjectPreview RingPreview;
			static SpriteObjectPreview GameObjectPreview;

			static rom::PaletteSet LevelPaletteSet;
			static bool export_result = false;

			static int level_index = 0;
			static rom::LevelDataOffsets level_data_offsets{ level_index };
			static bool preview_bonus_alt_palette = false;

			bool render_both = false;
			bool render_bg = false;
			bool render_fg = false;
			bool preview_intro_bg = false;
			bool preview_intro_fg = false;
			bool preview_intro_ship = false;
			bool preview_intro_water = false;
			bool preview_menu_bg = false;
			bool preview_menu_fg = false;
			bool preview_menu_combined = false;
			bool preview_bonus_bg = false;
			bool preview_bonus_fg = false;
			bool preview_bonus_combined = false;
			bool preview_sega_logo = false;
			bool preview_options = false;

			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("Levels"))
				{
					if (ImGui::Selectable("Toxic Caves"))
					{
						render_both = true;
						level_index = 0;
						level_data_offsets = { level_index };
					}

					if (ImGui::Selectable("Lava Powerhouse"))
					{
						render_both = true;
						level_index = 1;
						level_data_offsets = { level_index };
					}

					if (ImGui::Selectable("The Machine"))
					{
						render_both = true;
						level_index = 2;
						level_data_offsets = { level_index };
					}

					if (ImGui::Selectable("Showdown"))
					{
						render_both = true;
						level_index = 3;
						level_data_offsets = { level_index };
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Intro"))
				{
					if (ImGui::Selectable("Background"))
					{
						preview_intro_bg = true;
					}
					if (ImGui::Selectable("Foreground"))
					{
						preview_intro_fg = true;
					}
					if (ImGui::Selectable("Robotnik Ship"))
					{
						preview_intro_ship = true;
					}
					if (ImGui::Selectable("Water"))
					{
						preview_intro_water = true;
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Frontend"))
				{
					if (ImGui::Selectable("Background"))
					{
						preview_menu_bg = true;
					}
					if (ImGui::Selectable("Foreground"))
					{
						preview_menu_fg = true;
					}
					if (ImGui::Selectable("Combined"))
					{
						preview_menu_combined = true;
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Bonus"))
				{
					if (ImGui::Selectable("Background"))
					{
						preview_bonus_bg = true;
					}
					if (ImGui::Selectable("Foreground"))
					{
						preview_bonus_fg = true;
					}
					if (ImGui::Selectable("Combined"))
					{
						preview_bonus_combined = true;
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Other"))
				{
					if (ImGui::Selectable("Preview Sega Logo"))
					{
						preview_sega_logo = true;
					}

					if (ImGui::Selectable("Preview Options Menu"))
					{
						preview_options = true;
					}
					ImGui::EndMenu();
				}

				ImGui::SameLine();
				ImGui::Checkbox("Export", &export_result);
				ImGui::SameLine();
				ImGui::Checkbox("Alt palette", &preview_bonus_alt_palette);
				ImGui::EndMenuBar();
			}

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
				const Uint32 FGTilesetLayouts = m_owning_ui.GetROM().ReadUint32(level_data_offsets.foreground_tile_layout);

				LevelPaletteSet = *rom::PaletteSet::LoadFromROM(m_owning_ui.GetROM(), level_data_offsets.palette_set);

				const Uint16 LevelDimensionsX = m_owning_ui.GetROM().ReadUint16(level_data_offsets.tile_layout_width);
				const Uint16 LevelDimensionsY = m_owning_ui.GetROM().ReadUint16(level_data_offsets.tile_layout_height);

				RenderTileLayoutRequest request;

				request.tileset_address = BGTilesetOffsets;
				request.tile_brushes_address = BGTilesetBrushes;
				request.tile_brushes_address_end = FGTilesetLayouts;

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

				m_tile_layout_render_requests.emplace_back(std::move(request));
			}

			if (render_fg)
			{
				const auto& buffer = m_owning_ui.GetROM().m_buffer;
				const rom::LevelDataOffsets level_data_offsets{ level_index };
				const Uint32 FGTilesetOffsets = m_owning_ui.GetROM().ReadUint32(level_data_offsets.foreground_tileset);
				const Uint32 FGTilesetLayouts = m_owning_ui.GetROM().ReadUint32(level_data_offsets.foreground_tile_layout);
				const Uint32 FGTilesetBrushes = m_owning_ui.GetROM().ReadUint32(level_data_offsets.foreground_tile_brushes);
				const Uint32 BGTilesetBrushes = m_owning_ui.GetROM().ReadUint32(level_data_offsets.background_tile_brushes);

				LevelPaletteSet = *rom::PaletteSet::LoadFromROM(m_owning_ui.GetROM(), level_data_offsets.palette_set);

				const Uint16 LevelDimensionsX = m_owning_ui.GetROM().ReadUint16(level_data_offsets.tile_layout_width);
				const Uint16 LevelDimensionsY = m_owning_ui.GetROM().ReadUint16(level_data_offsets.tile_layout_height);

				RenderTileLayoutRequest request;

				request.tileset_address = FGTilesetOffsets;
				request.tile_brushes_address = FGTilesetBrushes;
				request.tile_brushes_address_end = BGTilesetBrushes;

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

				m_tile_layout_render_requests.emplace_back(std::move(request));
			}

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

				m_tile_layout_render_requests.emplace_back(std::move(request));
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
					m_tile_layout_render_requests.emplace_back(std::move(request));
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

					m_tile_layout_render_requests.emplace_back(std::move(request));
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

					m_tile_layout_render_requests.emplace_back(std::move(request));
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

					m_tile_layout_render_requests.emplace_back(std::move(request));
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
					m_tile_layout_render_requests.emplace_back(std::move(request));
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
					m_tile_layout_render_requests.emplace_back(std::move(request));
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
					m_tile_layout_render_requests.emplace_back(std::move(request));
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

					m_tile_layout_render_requests.emplace_back(std::move(request));
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

					m_tile_layout_render_requests.emplace_back(std::move(request));
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

				std::shared_ptr<const rom::Sprite> flipperSprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), flipper_sprite_offset[level_index]);
				FlipperPreview.sprite = SDLSurfaceHandle{ SDL_CreateSurface(44, 31, SDL_PIXELFORMAT_INDEX8) };
				FlipperPreview.palette = Renderer::CreateSDLPalette(*LevelPaletteSet.palette_lines[flipper_palette[level_index]]);
				SDL_SetSurfacePalette(FlipperPreview.sprite.get(), FlipperPreview.palette.get());
				SDL_ClearSurface(FlipperPreview.sprite.get(), 0.0f, 0.0f, 0.0f, 0.0f);
				SDL_SetSurfaceColorKey(FlipperPreview.sprite.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(FlipperPreview.sprite->format), nullptr, 0, static_cast<Uint8>(rom::Swatch::Pack(0, rom::Colour::levels_lookup[0x0], rom::Colour::levels_lookup[0x0])), 0, 0));
				flipperSprite->RenderToSurface(FlipperPreview.sprite.get());

				const Uint32 flippers_table_begin = m_owning_ui.GetROM().ReadUint32(flippers_ptr_table_offset + (4 * level_index));
				const Uint32 num_flippers = m_owning_ui.GetROM().ReadUint16(flippers_count_table_offset + (2 * level_index));

				m_flipper_instances.clear();
				m_flipper_instances.reserve(num_flippers);

				size_t current_table_offset = flippers_table_begin;

				for (size_t i = 0; i < num_flippers; ++i)
				{
					m_flipper_instances.emplace_back(rom::FlipperInstance::LoadFromROM(m_owning_ui.GetROM(), current_table_offset));
					current_table_offset = m_flipper_instances.back().rom_data.rom_offset_end;
				}
			}

			if (render_rings)
			{
				const static size_t ring_sprite_offset = 0x0000F6D8;

				std::shared_ptr<const rom::Sprite> LevelRingSprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), ring_sprite_offset);
				RingPreview.sprite = SDLSurfaceHandle{ SDL_CreateSurface(LevelRingSprite->GetBoundingBox().Width(), LevelRingSprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_INDEX8) };
				RingPreview.palette = Renderer::CreateSDLPalette(*LevelPaletteSet.palette_lines[3].get());
				SDL_SetSurfacePalette(RingPreview.sprite.get(), RingPreview.palette.get());
				SDL_ClearSurface(RingPreview.sprite.get(), 0.0f, 0.0f, 0.0f, 0.0f);
				SDL_SetSurfaceColorKey(RingPreview.sprite.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(RingPreview.sprite->format), nullptr, 0, 0, 0, 0));
				LevelRingSprite->RenderToSurface(RingPreview.sprite.get());

				m_ring_instances.clear();
				m_ring_instances.reserve(level_data_offsets.ring_instances.count);

				{
					size_t current_table_offset = level_data_offsets.ring_instances.offset;

					for (size_t i = 0; i < level_data_offsets.ring_instances.count; ++i)
					{
						m_ring_instances.emplace_back(rom::RingInstance::LoadFromROM(m_owning_ui.GetROM(), current_table_offset));
						current_table_offset = m_ring_instances.back().rom_data.rom_offset_end;
					}
				}
				/////////////

				const static size_t game_obj_sprite_offset = 0x0000F6D8;

				std::shared_ptr<const rom::Sprite> LevelGameObjSprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), game_obj_sprite_offset);
				GameObjectPreview.sprite = SDLSurfaceHandle{ SDL_CreateSurface(LevelGameObjSprite->GetBoundingBox().Width(), LevelGameObjSprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_RGBA32) };
				GameObjectPreview.palette = Renderer::CreateSDLPalette(*LevelPaletteSet.palette_lines[3].get());
				SDL_SetSurfacePalette(GameObjectPreview.sprite.get(), GameObjectPreview.palette.get());
				SDL_ClearSurface(GameObjectPreview.sprite.get(), 255, 255, 255, 255);
				SDL_SetSurfaceColorKey(GameObjectPreview.sprite.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(GameObjectPreview.sprite->format), nullptr, 255, 0, 0, 255));
				//LevelGameObjSprite->RenderToSurface(GameObjectPreview.sprite.get());

				m_preview_game_objects.clear();
				m_game_obj_instances.clear();
				m_anim_sprite_instances.clear();
				m_game_obj_instances.reserve(level_data_offsets.object_instances.count);

				{
					size_t current_table_offset = level_data_offsets.object_instances.offset;

					for (size_t i = 0; i < level_data_offsets.object_instances.count; ++i)
					{
						m_game_obj_instances.emplace_back(rom::GameObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), current_table_offset));
						const rom::Ptr32 anim_def_offset = m_game_obj_instances.back().animation_definition;
						rom::AnimObjectDefinition anim_obj = rom::AnimObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), anim_def_offset);

						auto found_sprite = std::find_if(std::begin(m_anim_sprite_instances), std::end(m_anim_sprite_instances), [&anim_obj](const AnimSpriteEntry& entry)
							{
								return anim_obj.sprite_table == entry.sprite_table && anim_obj.starting_animation == entry.anim_sequence->rom_data.rom_offset;
							});

						if (found_sprite == std::end(m_anim_sprite_instances))
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
								entry.palette = Renderer::CreateSDLPalette(*LevelPaletteSet.palette_lines.at(anim_obj.palette_index % 4).get());
								SDL_SetSurfacePalette(new_sprite_surface.get(), entry.palette.get());
								SDL_ClearSurface(new_sprite_surface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
								SDL_SetSurfaceColorKey(new_sprite_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(new_sprite_surface->format), nullptr, 0, 0, 0, 0));
								first_frame_sprite->RenderToSurface(new_sprite_surface.get());

								entry.sprite_surface = std::move(new_sprite_surface);
							}


							m_anim_sprite_instances.emplace_back(std::move(entry));
						}
						current_table_offset = m_game_obj_instances.back().rom_data.rom_offset_end;
					}
				}
			}

			const bool will_be_rendering_preview = m_tile_layout_render_requests.empty() == false;
			const bool export_combined = m_tile_layout_render_requests.size() > 1;
			static std::optional<RenderTileLayoutRequest> current_preview_data;
			if (will_be_rendering_preview && export_combined == false)
			{
				current_preview_data = m_tile_layout_render_requests.front();
			}

			char combined_buffer[128];
			if (export_combined)
			{
				sprintf_s(combined_buffer, "%s_combined", m_tile_layout_render_requests.front().layout_type_name.c_str());
			}
			const std::string combined_layout_name = export_combined ? m_tile_layout_render_requests.front().layout_layout_name : "";
			const std::string combined_type_name = export_combined ? combined_buffer : "";
			SDLSurfaceHandle layout_preview_bg_surface;
			SDLSurfaceHandle layout_preview_fg_surface;
			if (will_be_rendering_preview)
			{
				bool will_require_mirror = false;
				int largest_width = std::numeric_limits<int>::min();
				int largest_height = std::numeric_limits<int>::min();

				for (const auto& request : m_tile_layout_render_requests)
				{
					largest_width = std::max<int>(request.tile_layout_width * request.tile_brush_width, largest_width);
					largest_height = std::max<int>(request.tile_layout_height * request.tile_brush_height, largest_height);
					will_require_mirror |= request.draw_mirrored_layout;
				}

				if (will_require_mirror)
				{
					largest_width *= 2;
				}

				const RenderTileLayoutRequest& request = m_tile_layout_render_requests.front();
				layout_preview_bg_surface = SDLSurfaceHandle{ SDL_CreateSurface(rom::TileSet::s_tile_width * largest_width, rom::TileSet::s_tile_height * largest_height, SDL_PIXELFORMAT_RGBA32) };
				SDL_ClearSurface(layout_preview_bg_surface.get(), 0.0f, 0.0f, 0.0f, 0.0f);

				layout_preview_fg_surface = SDLSurfaceHandle{ SDL_CreateSurface(rom::TileSet::s_tile_width * largest_width, rom::TileSet::s_tile_height * largest_height, SDL_PIXELFORMAT_RGBA32) };
				SDL_ClearSurface(layout_preview_fg_surface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
			}

			while (m_tile_layout_render_requests.empty() == false)
			{
				const RenderTileLayoutRequest& request = m_tile_layout_render_requests.front();
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
				m_tile_brushes_previews.reserve(brushes.capacity());
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
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_bg_surface.get(), &target_rect);
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
						SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_bg_surface.get(), &target_rect);
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
					assert(IMG_SavePNG(layout_preview_bg_surface.get(), export_path.generic_u8string().c_str()));
				}

				m_tile_layout_render_requests.erase(std::begin(m_tile_layout_render_requests));
			}

			if (render_flippers)
			{
				for (size_t i = 0; i < m_flipper_instances.size(); ++i)
				{
					const rom::FlipperInstance& flipper = m_flipper_instances[i];

					SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(FlipperPreview.sprite.get()) };
					int x_off = -24;
					if (flipper.is_x_flipped)
					{
						SDL_FlipSurface(temp_surface.get(), SDL_FLIP_HORIZONTAL);
						x_off = -20;
					}

					SDL_Rect target_rect{ flipper.x_pos + x_off, flipper.y_pos - 31, 44, 31 };
					SDL_SetSurfaceColorKey(temp_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_fg_surface.get(), &target_rect);
				}
			}

			if (render_rings)
			{
				for (size_t i = 0; i < m_ring_instances.size(); ++i)
				{
					const rom::RingInstance& ring = m_ring_instances[i];

					SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(RingPreview.sprite.get()) };
					SDL_Rect target_rect{ ring.x_pos - 8, ring.y_pos - 16, 0x10, 0x10 };
					SDL_SetSurfaceColorKey(temp_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_fg_surface.get(), &target_rect);
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
				static rom::Colour collision_box_colour = { 0, 255, 255, 255 };

				for (size_t i = 0; i < m_game_obj_instances.size(); ++i)
				{
					const rom::GameObjectDefinition& game_obj = m_game_obj_instances[i];
					rom::AnimObjectDefinition anim_obj = rom::AnimObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), m_game_obj_instances[i].animation_definition);

					auto anim_entry = std::find_if(std::begin(m_anim_sprite_instances), std::end(m_anim_sprite_instances), [&anim_obj](const AnimSpriteEntry& entry)
						{
							return entry.anim_sequence->rom_data.rom_offset == anim_obj.starting_animation;
						});

					if (anim_entry != std::end(m_anim_sprite_instances) && anim_entry->sprite_surface != nullptr)
					{
						const AnimSpriteEntry& animSpriteEntry = *anim_entry;
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
						SDL_BlitSurfaceScaled(temp_sprite_surface.get(), nullptr, layout_preview_bg_surface.get(), &sprite_target_rect, SDL_SCALEMODE_NEAREST);

						std::unique_ptr<UIGameObject> new_obj = std::make_unique<UIGameObject>();
						new_obj->obj_definition = game_obj;
						new_obj->pos = ImVec2{ static_cast<float>(sprite_target_rect.x), static_cast<float>(sprite_target_rect.y) };
						new_obj->dimensions = ImVec2{ static_cast<float>(sprite_target_rect.w) , static_cast<float>(sprite_target_rect.h) };
						new_obj->ui_sprite = command_sprite_came_from.ui_frame_sprite;
						new_obj->sprite_table_address = animSpriteEntry.sprite_table;
						m_preview_game_objects.emplace_back(std::move(new_obj));
					}
					else
					{
						SDLSurfaceHandle temp_surface{ SDL_ScaleSurface(GameObjectPreview.sprite.get(), game_obj.collision_width, game_obj.collision_height, SDL_SCALEMODE_NEAREST) };

						auto cutting_surface = SDLSurfaceHandle{ SDL_CreateSurface(temp_surface->w, temp_surface->h, SDL_PIXELFORMAT_RGBA32) };
						SDL_ClearSurface(temp_surface.get(), bbox_colours[game_obj.type_id % std::size(bbox_colours)].r / 256.0f, bbox_colours[game_obj.type_id % std::size(bbox_colours)].g / 256.0f, bbox_colours[game_obj.type_id % std::size(bbox_colours)].b / 256.0f, 255.0f);
						SDL_ClearSurface(cutting_surface.get(), 255.0f, 0.0f, 0.0f, 255.0f);
						const SDL_Rect cutting_target_rect{ 2,2,temp_surface->w - 4, temp_surface->h - 4 };
						SDL_BlitSurfaceScaled(cutting_surface.get(), nullptr, temp_surface.get(), &cutting_target_rect, SDL_SCALEMODE_NEAREST);

						SDL_Rect target_rect{ game_obj.x_pos - game_obj.collision_width / 2, game_obj.y_pos - game_obj.collision_height, game_obj.collision_width, game_obj.collision_height };
						SDL_SetSurfaceColorKey(temp_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 255, 0, 0, 255));
						SDL_BlitSurfaceScaled(temp_surface.get(), nullptr, layout_preview_bg_surface.get(), &target_rect, SDL_SCALEMODE_NEAREST);
					}
				}
			}

			if (export_result && export_combined)
			{
				static char path_buffer[4096];

				sprintf_s(path_buffer, "spinball_%s.png", combined_type_name.c_str());
				std::filesystem::path export_path = m_owning_ui.GetSpriteExportPath().append(path_buffer);
				if (export_combined)
				{
					SDLSurfaceHandle combined{ SDL_DuplicateSurface(layout_preview_bg_surface.get()) };
					SDL_BlitSurface(layout_preview_fg_surface.get(), nullptr, combined.get(), nullptr);
					assert(IMG_SavePNG(combined.get(), export_path.generic_u8string().c_str()));
				}
				else
				{
					assert(IMG_SavePNG(layout_preview_bg_surface.get(), export_path.generic_u8string().c_str()));
				}
			}

			if (will_be_rendering_preview)
			{
				m_tile_layout_preview_bg = Renderer::RenderToTexture(layout_preview_bg_surface.get());
				m_tile_layout_preview_fg = Renderer::RenderToTexture(layout_preview_fg_surface.get());
			}

			constexpr const float zoom = 1.0f;

			static int selected_collision_tile_index = -1;
			ImGui::SliderInt("Collision Tile", &selected_collision_tile_index, -1, 0x100 - 1);

			if (ImGui::BeginChild("Preview info Area"))
			{
				constexpr size_t preview_brushes_per_row = 32;
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
						if (palette != nullptr)
						{
							DrawPaletteSwatchPreview(*palette);
						}
					}
					ImGui::TreePop();
				}

				static int current_page = 0;
				if (m_tile_brushes_previews.empty() == false && ImGui::TreeNode("Brush Previews"))
				{
					constexpr size_t num_previews_per_page = 16 * 8;
					ImGui::BeginDisabled((current_page - 1) < 0);
					if (ImGui::Button("Previous Page"))
					{
						current_page = std::max(0, current_page - 1);
					}
					ImGui::EndDisabled();
					ImGui::SameLine();
					ImGui::BeginDisabled((current_page + 1) * num_previews_per_page >= m_tile_brushes_previews.size());
					if (ImGui::Button("Next Page"))
					{
						current_page = std::min<int>((static_cast<int>(m_tile_brushes_previews.size()) / num_previews_per_page), current_page + 1);
					}
					ImGui::EndDisabled();
					for (size_t i = current_page * num_previews_per_page; i < std::min<size_t>((current_page + 1) * num_previews_per_page, m_tile_brushes_previews.size()); ++i)
					{
						TileBrushPreview& preview_brush = m_tile_brushes_previews[i];
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
						}
					}
					ImGui::TreePop();
				}

				if (m_tile_layout_preview_bg != nullptr || m_tile_layout_preview_fg != nullptr)
				{
					struct WorkingSpline
					{
						rom::Ptr32 offset;
						BoundingBox bbox;
					};

					static UIGameObject* selected_game_obj = nullptr;
					static std::optional<WorkingSpline> working_spline;
					const ImVec2 origin = ImGui::GetCursorPos();
					const ImVec2 screen_origin = ImGui::GetCursorScreenPos();
					ImGui::Image((ImTextureID)m_tile_layout_preview_bg.get(), ImVec2(static_cast<float>(m_tile_layout_preview_bg->w) * zoom, static_cast<float>(m_tile_layout_preview_bg->h) * zoom));

					ImGui::SetCursorPos(origin);
					ImGui::Image((ImTextureID)m_tile_layout_preview_fg.get(), ImVec2(static_cast<float>(m_tile_layout_preview_fg->w) * zoom, static_cast<float>(m_tile_layout_preview_fg->h) * zoom));

					// Visualise collision vectors
					constexpr Uint32 num_collision_data = 0x100;
					constexpr int tile_width = 128;
					constexpr int num_tiles_x = 16;
					constexpr int size_of_preview_collision_boxes = 4;
					constexpr int half_size_of_preview_collision_boxes = size_of_preview_collision_boxes / 2;

					const rom::Ptr32 terrain_collision_data = m_owning_ui.GetROM().ReadUint32(level_data_offsets.collision_data_terrain);

					std::vector<Uint16> collision_data_offsets;
					collision_data_offsets.reserve(num_collision_data);

					for (rom::Ptr32 i = 0; i < num_collision_data; ++i)
					{
						collision_data_offsets.emplace_back(m_owning_ui.GetROM().ReadUint16(terrain_collision_data + (i * 2)));
					}

					// Terrain Collision

					const rom::Ptr32 collision_index_start = selected_collision_tile_index != -1 ? selected_collision_tile_index : 0;
					const rom::Ptr32 collision_index_end = selected_collision_tile_index != -1 ? selected_collision_tile_index + 1 : static_cast<rom::Ptr32>(collision_data_offsets.size()-1);

					if(selected_collision_tile_index != -1)
					{
						const int collision_tile_origin_x = (static_cast<int>(selected_collision_tile_index) % num_tiles_x) * tile_width;
						const int collision_tile_origin_y = (static_cast<int>(selected_collision_tile_index) / num_tiles_x) * tile_width;

						ImGui::GetWindowDrawList()->AddRect(ImVec2{ static_cast<float>(screen_origin.x + collision_tile_origin_x), static_cast<float>(screen_origin.y + collision_tile_origin_y) }, ImVec2{ static_cast<float>(screen_origin.x + collision_tile_origin_x + tile_width), static_cast<float>(screen_origin.y + collision_tile_origin_y + tile_width) }, ImGui::GetColorU32(ImVec4{ 64,64,64,255 }), 0, ImDrawFlags_None, 1.0f);
					}

					for (rom::Ptr32 i = collision_index_start; i < collision_index_end; ++i)
					{
						const rom::Ptr32 start_offset = terrain_collision_data + (collision_data_offsets[i] * 2);
						const rom::Ptr32 data_start_offset = start_offset + 2;
						const rom::Ptr32 end_offset = terrain_collision_data + (collision_data_offsets[i + 1] * 2);
						const Uint32 num_bytes = end_offset - start_offset;

						const int collision_tile_origin_x = (static_cast<int>(i) % num_tiles_x) * tile_width;
						const int collision_tile_origin_y = (static_cast<int>(i) / num_tiles_x) * tile_width;

						BoundingBox bbox;
						bbox.min.x = collision_tile_origin_x;
						bbox.min.y = collision_tile_origin_y;
						bbox.max.x = collision_tile_origin_x + tile_width;
						bbox.max.y = collision_tile_origin_y + tile_width;

						const Uint16 num_objects = static_cast<int>(m_owning_ui.GetROM().ReadUint16(start_offset));


						for (Uint16 s = 0; s < num_objects; ++s)
						{
							rom::Ptr32 short_offset = s * 12;

							BoundingBox bbox2;
							bbox2.min.x = m_owning_ui.GetROM().ReadUint16(data_start_offset + short_offset + 0);
							bbox2.min.y = m_owning_ui.GetROM().ReadUint16(data_start_offset + short_offset + 2);
							bbox2.max.x = m_owning_ui.GetROM().ReadUint16(data_start_offset + short_offset + 4);
							bbox2.max.y = m_owning_ui.GetROM().ReadUint16(data_start_offset + short_offset + 6);

							const Uint16 object_type_flags = m_owning_ui.GetROM().ReadUint16(data_start_offset + short_offset + 8);
							const Uint16 extra_info = m_owning_ui.GetROM().ReadUint16(data_start_offset + short_offset + 10);

							enum class CollisionObjectType
							{
								Spline,
								BBox
							};

							const bool is_bbox = (object_type_flags & 0x8000);
							const bool is_teleporter = (object_type_flags & 0x1000);
							const bool is_unknown = !is_bbox && !is_teleporter && object_type_flags != 0;

							ImVec4 colour{ 192,192,0,255 };
							if (is_bbox)
							{
								colour = ImVec4(255, 0, 255, 255);
							}

							if (is_teleporter)
							{
								colour = ImVec4(0, 255, 255, 255);
							}

							if (is_unknown)
							{
								colour = ImVec4(128, 128, 255, 255);
							}

							if (is_bbox)
							{
								ImGui::GetWindowDrawList()->AddCircle(
									ImVec2{ static_cast<float>(screen_origin.x + bbox2.min.x), static_cast<float>(screen_origin.y + bbox2.min.y) },
									static_cast<float>(bbox2.max.x), ImGui::GetColorU32(colour), 16, 1.0f);

									ImGui::GetWindowDrawList()->AddRect(
										ImVec2{ static_cast<float>(screen_origin.x + bbox2.min.x - 2), static_cast<float>(screen_origin.y + bbox2.min.y - 2) },
										ImVec2{ static_cast<float>(screen_origin.x + bbox2.min.x + 3), static_cast<float>(screen_origin.y + bbox2.min.y + 3) },
									ImGui::GetColorU32(colour), 0, ImDrawFlags_None, 1.0f);
							}
							else
							{
								ImGui::GetWindowDrawList()->AddLine(
									ImVec2{ static_cast<float>(screen_origin.x + bbox2.min.x), static_cast<float>(screen_origin.y + bbox2.min.y) },
									ImVec2{ static_cast<float>(screen_origin.x + bbox2.max.x), static_cast<float>(screen_origin.y + bbox2.max.y) },
									ImGui::GetColorU32(colour), 2.0f);

								ImGui::SetCursorPos(ImVec2{ origin.x + bbox2.min.x, origin.y + bbox2.min.y });
								ImGui::Dummy(ImVec2{ static_cast<float>(bbox.Width()), static_cast<float>(bbox.Height())});

								BoundingBox fixed_bbox;
								fixed_bbox.min.x = std::min(bbox2.min.x, bbox2.max.x) - 1;
								fixed_bbox.max.x = std::max(bbox2.min.x, bbox2.max.x) + 1;
								fixed_bbox.min.y = std::min(bbox2.min.y, bbox2.max.y) - 1;
								fixed_bbox.max.y = std::max(bbox2.min.y, bbox2.max.y) + 1;

								if (ImGui::IsMouseHoveringRect(
									ImVec2{ static_cast<float>(screen_origin.x + fixed_bbox.min.x), static_cast<float>(screen_origin.y + fixed_bbox.min.y) },
									ImVec2{ static_cast<float>(screen_origin.x + fixed_bbox.max.x), static_cast<float>(screen_origin.y + fixed_bbox.max.y) }))
								{
									ImGui::GetForegroundDrawList()->AddRect(
										ImVec2{ static_cast<float>(screen_origin.x + fixed_bbox.min.x), static_cast<float>(screen_origin.y + fixed_bbox.min.y) },
										ImVec2{ static_cast<float>(screen_origin.x + fixed_bbox.max.x), static_cast<float>(screen_origin.y + fixed_bbox.max.y) },
										ImGui::GetColorU32(ImVec4{ 255,0,255,255 }), 0, ImDrawFlags_None, 2.0f);

									if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
									{
										ImGui::OpenPopup("spline_popup");
										WorkingSpline new_spline;
										new_spline.offset = data_start_offset + short_offset;
										new_spline.bbox = bbox2;
										working_spline.emplace(new_spline);
									}
								}
							}
						}
					}

					for (std::unique_ptr<UIGameObject>& game_obj : m_preview_game_objects)
					{
						ImGui::SetCursorPos(ImVec2{ origin.x + game_obj->pos.x, origin.y + game_obj->pos.y });
						ImGui::Dummy(game_obj->dimensions);
						if (ImGui::IsPopupOpen("obj_popup") == false && ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
						{
							selected_game_obj = nullptr;
							ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImVec4{ 0,192,0,255 }), 1.0f, 0, 2);

							for (Uint32 sector_index = 0; sector_index < 48-1; ++sector_index)
							{
								const rom::Ptr32 anim_obj_collision_data = m_owning_ui.GetROM().ReadUint32(level_data_offsets.camera_activation_sector_anim_obj_ids);
								const Uint8 data_offset = m_owning_ui.GetROM().ReadUint8(anim_obj_collision_data + sector_index);
								const rom::Ptr32 activation_sector_data = anim_obj_collision_data + data_offset;

								constexpr Uint32 cam_width = 0x168;
								constexpr Uint32 cam_height = 0x108;
								constexpr Uint32 num_sectors_x = 6;

								const Uint32 instance_id = game_obj->obj_definition.instance_id;
								const Uint32 data_offset_end = m_owning_ui.GetROM().ReadUint8(anim_obj_collision_data + sector_index + 1);
								const Uint32 num_obj_ids = data_offset_end - data_offset;

								for (Uint32 i = 0; i < num_obj_ids; ++i)
								{
									const Uint32 obj_id = m_owning_ui.GetROM().ReadUint8(anim_obj_collision_data + data_offset + i);
									if (obj_id == game_obj->obj_definition.instance_id)
									{
										BoundingBox sector_bbox;
										sector_bbox.min.x = (sector_index % num_sectors_x) * cam_width;
										sector_bbox.min.y = (sector_index / num_sectors_x) * cam_height;
										sector_bbox.max.x = sector_bbox.min.x + cam_width;
										sector_bbox.max.y = sector_bbox.min.y + cam_height;

										ImGui::GetWindowDrawList()->AddRect(
											ImVec2{ static_cast<float>(screen_origin.x + sector_bbox.min.x), static_cast<float>(screen_origin.y + sector_bbox.min.y) },
											ImVec2{ static_cast<float>(screen_origin.x + sector_bbox.max.x), static_cast<float>(screen_origin.y + sector_bbox.max.y) },
											ImGui::GetColorU32(ImVec4{ 255,64,64,255 }), 0, ImDrawFlags_None, 1.0f);
									}
								}
							}

							for (Uint32 sector_index = 0; sector_index < 256 - 1; ++sector_index)
							{
								const rom::Ptr32 anim_obj_collision_data = level_data_offsets.collision_tile_obj_ids.offset;
								const Uint16 data_offset = m_owning_ui.GetROM().ReadUint16(anim_obj_collision_data + sector_index*2);
								const rom::Ptr32 activation_sector_data = anim_obj_collision_data + data_offset;

								const Uint32 instance_id = game_obj->obj_definition.instance_id;
								const Uint32 data_offset_end = m_owning_ui.GetROM().ReadUint16(anim_obj_collision_data + sector_index*2 + 2);
								const Uint32 num_obj_ids = data_offset_end - data_offset;

								for (Uint32 i = 0; i < num_obj_ids; ++i)
								{
									const Uint32 obj_id = m_owning_ui.GetROM().ReadUint16(anim_obj_collision_data + (data_offset*2) + (i*2));
									if (obj_id == game_obj->obj_definition.instance_id)
									{
										BoundingBox sector_bbox;
										sector_bbox.min.x = (sector_index % num_tiles_x) * tile_width;
										sector_bbox.min.y = (sector_index / num_tiles_x) * tile_width;
										sector_bbox.max.x = sector_bbox.min.x + tile_width;
										sector_bbox.max.y = sector_bbox.min.y + tile_width;

										ImGui::GetWindowDrawList()->AddRect(
											ImVec2{ static_cast<float>(screen_origin.x + sector_bbox.min.x), static_cast<float>(screen_origin.y + sector_bbox.min.y) },
											ImVec2{ static_cast<float>(screen_origin.x + sector_bbox.max.x), static_cast<float>(screen_origin.y + sector_bbox.max.y) },
											ImGui::GetColorU32(ImVec4{ 255,0,255,255 }), 0, ImDrawFlags_None, 2.0f);
									}
								}
							}

							if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
							{
								ImGui::OpenPopup("obj_popup");
								selected_game_obj = game_obj.get();
							}
							else if (ImGui::BeginTooltip())
							{
								ImGui::Text("Sprite Table: 0x%04X", game_obj->sprite_table_address);
								ImGui::Text("Instance ID: 0x%02X", game_obj->obj_definition.instance_id);
								ImGui::Image((ImTextureID)game_obj->ui_sprite->texture.get(), ImVec2(static_cast<float>(game_obj->ui_sprite->texture->w) * 2.0f, static_cast<float>(game_obj->ui_sprite->texture->h) * 2.0f));
								ImGui::EndTooltip();
							}
						}
					}

					if (selected_game_obj == nullptr && working_spline.has_value() == false)
					{
						ImGui::CloseCurrentPopup();
					}
					
					if (ImGui::BeginPopup("obj_popup"))
					{
						const auto origin_offset = selected_game_obj->ui_sprite != nullptr ? selected_game_obj->ui_sprite->sprite->GetOriginOffsetFromMinBounds() : Point{ 0,0 };
						int pos[2] = { static_cast<int>(selected_game_obj->pos.x + origin_offset.x), static_cast<int>(selected_game_obj->pos.y + origin_offset.y) };
						if (ImGui::InputInt2("Object Pos", pos, ImGuiInputTextFlags_EnterReturnsTrue))
						{
							selected_game_obj->obj_definition.x_pos = pos[0];
							selected_game_obj->obj_definition.y_pos = pos[1];
							selected_game_obj->obj_definition.SaveToROM(m_owning_ui.GetROM());
							selected_game_obj = nullptr;
							ImGui::CloseCurrentPopup();
						}
						ImGui::EndPopup();
					}
					
					if (ImGui::BeginPopup("spline_popup"))
					{
						if (working_spline.has_value() == false)
						{
							ImGui::CloseCurrentPopup();
						}
						else
						{
							int spline_min[2] = { static_cast<int>(working_spline->bbox.min.x), static_cast<int>(working_spline->bbox.min.y) };
							int spline_max[2] = { static_cast<int>(working_spline->bbox.max.x), static_cast<int>(working_spline->bbox.max.y) };

							ImGui::InputInt2("Spline Min", spline_min);
							ImGui::InputInt2("Spline Max", spline_max);

							working_spline->bbox.min.x = spline_min[0];
							working_spline->bbox.min.y = spline_min[1];
							working_spline->bbox.max.x = spline_max[0];
							working_spline->bbox.max.y = spline_max[1];

							if (ImGui::Button("Confirm"))
							{
								m_owning_ui.GetROM().WriteUint16(working_spline->offset + 0, spline_min[0]);
								m_owning_ui.GetROM().WriteUint16(working_spline->offset + 2, spline_min[1]);
								m_owning_ui.GetROM().WriteUint16(working_spline->offset + 4, spline_max[0]);
								m_owning_ui.GetROM().WriteUint16(working_spline->offset + 6, spline_max[1]);

								working_spline.reset();
								ImGui::CloseCurrentPopup();
							}

							if (ImGui::Button("Cancel"))
							{
								working_spline.reset();
								ImGui::CloseCurrentPopup();
							}
						}
						ImGui::EndPopup();
					}
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}
}