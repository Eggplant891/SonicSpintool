#include "ui/ui_palette_viewer.h"

#include "SDL3/SDL_stdinc.h"
#include "ui/ui_editor.h"
#include "render.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>


namespace spintool
{
	namespace
	{
		constexpr const char* kColourEditorPopup = "Edit Palette Colour";
		constexpr int kHueShadeSamples = 29;

		constexpr Uint32 kLevelPaletteOffset = 0x00000DFCU;
		constexpr Uint32 kLevelPaletteCount = 48U;
		constexpr Uint32 kSegaLogoPaletteOffset = 0x000993FAU;
		constexpr Uint32 kSegaLogoPaletteCount = 2U;
		constexpr Uint32 kIntroPaletteOffset = 0x000A1388U;
		constexpr Uint32 kIntroPaletteCount = 4U;
		constexpr Uint32 kTitleScreenPaletteOffset = 0x0009BD3AU;
		constexpr Uint32 kTitleScreenPaletteCount = 4U;

		enum class PaletteGroup
		{
			Level,
			SegaLogo,
			Introduction,
			TitleScreen,
			Other
		};

		bool IsPaletteLineInRange(
			const Uint32 offset,
			const Uint32 range_start,
			const Uint32 line_count
		)
		{
			if (offset < range_start)
			{
				return false;
			}

			const Uint32 relative = offset - range_start;
			return relative < line_count * rom::Palette::s_palette_size_on_rom &&
				relative % rom::Palette::s_palette_size_on_rom == 0U;
		}

		PaletteGroup GetPaletteGroup(const Uint32 offset)
		{
			if (IsPaletteLineInRange(offset, kLevelPaletteOffset, kLevelPaletteCount))
			{
				return PaletteGroup::Level;
			}
			if (IsPaletteLineInRange(offset, kSegaLogoPaletteOffset, kSegaLogoPaletteCount))
			{
				return PaletteGroup::SegaLogo;
			}
			if (IsPaletteLineInRange(offset, kIntroPaletteOffset, kIntroPaletteCount))
			{
				return PaletteGroup::Introduction;
			}
			if (IsPaletteLineInRange(offset, kTitleScreenPaletteOffset, kTitleScreenPaletteCount))
			{
				return PaletteGroup::TitleScreen;
			}
			return PaletteGroup::Other;
		}

		const char* PaletteGroupName(const PaletteGroup group)
		{
			switch (group)
			{
				case PaletteGroup::Level:
					return "Level Palettes";
				case PaletteGroup::SegaLogo:
					return "SEGA Logo";
				case PaletteGroup::Introduction:
					return "Introduction";
				case PaletteGroup::TitleScreen:
					return "Title Screen";
				case PaletteGroup::Other:
				default:
					return "Other Palettes";
			}
		}

		Uint32 PaletteLineNumber(const rom::Palette& palette, const PaletteGroup group)
		{
			Uint32 range_start = palette.offset;
			switch (group)
			{
				case PaletteGroup::Level:
					range_start = kLevelPaletteOffset;
					break;
				case PaletteGroup::SegaLogo:
					range_start = kSegaLogoPaletteOffset;
					break;
				case PaletteGroup::Introduction:
					range_start = kIntroPaletteOffset;
					break;
				case PaletteGroup::TitleScreen:
					range_start = kTitleScreenPaletteOffset;
					break;
				case PaletteGroup::Other:
				default:
					return 0U;
			}

			return (palette.offset - range_start) / rom::Palette::s_palette_size_on_rom;
		}

		enum class ColourSelectionMode
		{
			Hue,
			Grayscale,
			RGBConversion
		};

		struct HueShade
		{
			Uint16 packed = 0;
			float luminance = 0.0f;
		};

		struct PaletteColourEditorState
		{
			int palette_index = -1;
			int swatch_index = -1;
			Uint16 reference_packed = 0;
			Uint16 current_packed = 0;
			Uint16 pending_packed = 0;
			bool reference_loaded = false;
			bool reference_available = false;
			float selected_hue = 0.0f;
			float generated_hue = -1.0f;
			ColourSelectionMode mode = ColourSelectionMode::Hue;
			int rgb_red = 0;
			int rgb_green = 0;
			int rgb_blue = 0;
			std::vector<HueShade> hue_shades;
		};

		PaletteColourEditorState g_colour_editor;

		ImVec4 PackedToImVec4(const Uint16 packed)
		{
			const rom::Swatch swatch{ packed };
			return swatch.AsImColor().Value;
		}

		float PackedLuminance(const Uint16 packed)
		{
			const rom::Colour colour = rom::Swatch{ packed }.GetUnpacked();
			return (
				0.2126f * static_cast<float>(colour.r) +
				0.7152f * static_cast<float>(colour.g) +
				0.0722f * static_cast<float>(colour.b)
			) / 255.0f;
		}

		void SyncRGBInputsFromPacked(PaletteColourEditorState& editor)
		{
			const rom::Colour colour = rom::Swatch{ editor.pending_packed }.GetUnpacked();
			editor.rgb_red = static_cast<int>(colour.r);
			editor.rgb_green = static_cast<int>(colour.g);
			editor.rgb_blue = static_cast<int>(colour.b);
		}

		void SetPendingPacked(PaletteColourEditorState& editor, const Uint16 packed)
		{
			editor.pending_packed = packed;
			SyncRGBInputsFromPacked(editor);
		}

		void SetHueFromPacked(PaletteColourEditorState& editor, const Uint16 packed)
		{
			const rom::Colour colour = rom::Swatch{ packed }.GetUnpacked();
			float hue = 0.0f;
			float saturation = 0.0f;
			float value = 0.0f;
			ImGui::ColorConvertRGBtoHSV(
				static_cast<float>(colour.r) / 255.0f,
				static_cast<float>(colour.g) / 255.0f,
				static_cast<float>(colour.b) / 255.0f,
				hue,
				saturation,
				value
			);

			// Greys do not contain a meaningful hue, so retain the current hue.
			if (saturation > 0.01f && value > 0.01f)
			{
				editor.selected_hue = hue;
				editor.generated_hue = -1.0f;
			}
		}

		ImVec4 HuePreviewColour(const float hue)
		{
			float red = 0.0f;
			float green = 0.0f;
			float blue = 0.0f;
			ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, red, green, blue);
			return ImVec4{ red, green, blue, 1.0f };
		}

		void HueAndLightnessToRGB(
			const float hue,
			const float lightness,
			float& red,
			float& green,
			float& blue
		)
		{
			float pure_red = 0.0f;
			float pure_green = 0.0f;
			float pure_blue = 0.0f;
			ImGui::ColorConvertHSVtoRGB(
				hue,
				1.0f,
				1.0f,
				pure_red,
				pure_green,
				pure_blue
			);

			if (lightness <= 0.5f)
			{
				const float scale = lightness * 2.0f;
				red = pure_red * scale;
				green = pure_green * scale;
				blue = pure_blue * scale;
			}
			else
			{
				const float white_mix = (lightness - 0.5f) * 2.0f;
				red = pure_red + ((1.0f - pure_red) * white_mix);
				green = pure_green + ((1.0f - pure_green) * white_mix);
				blue = pure_blue + ((1.0f - pure_blue) * white_mix);
			}
		}

		void RefreshHueShades(PaletteColourEditorState& editor)
		{
			if (!editor.hue_shades.empty() &&
				std::fabs(editor.generated_hue - editor.selected_hue) <= 0.0001f)
			{
				return;
			}

			editor.generated_hue = editor.selected_hue;
			editor.hue_shades.clear();
			editor.hue_shades.reserve(kHueShadeSamples);

			for (int index = 0; index < kHueShadeSamples; ++index)
			{
				const float lightness = static_cast<float>(index) /
					static_cast<float>(kHueShadeSamples - 1);
				float red = 0.0f;
				float green = 0.0f;
				float blue = 0.0f;
				HueAndLightnessToRGB(
					editor.selected_hue,
					lightness,
					red,
					green,
					blue
				);

				const Uint16 packed = rom::Swatch::Pack(red, green, blue);
				const auto duplicate = std::find_if(
					editor.hue_shades.begin(),
					editor.hue_shades.end(),
					[packed](const HueShade& shade)
					{
						return shade.packed == packed;
					}
				);
				if (duplicate == editor.hue_shades.end())
				{
					editor.hue_shades.push_back(
						HueShade{ packed, PackedLuminance(packed) }
					);
				}
			}

			std::sort(
				editor.hue_shades.begin(),
				editor.hue_shades.end(),
				[](const HueShade& lhs, const HueShade& rhs)
				{
					if (lhs.luminance == rhs.luminance)
					{
						return lhs.packed < rhs.packed;
					}
					return lhs.luminance < rhs.luminance;
				}
			);
		}

		std::vector<HueShade> BuildGrayscaleShades()
		{
			std::vector<HueShade> shades;
			shades.reserve(15);

			// Level F has the same visible intensity as level E in this project,
			// so display one white swatch instead of a duplicate.
			for (int level = 0; level <= 14; ++level)
			{
				const Uint16 packed = static_cast<Uint16>(
					(level << 8U) | (level << 4U) | level
				);
				shades.push_back(HueShade{ packed, PackedLuminance(packed) });
			}
			return shades;
		}

		Uint16 FindShadeAtLuminance(
			PaletteColourEditorState& editor,
			const float target_luminance
		)
		{
			RefreshHueShades(editor);
			if (editor.hue_shades.empty())
			{
				return editor.pending_packed;
			}

			const auto closest = std::min_element(
				editor.hue_shades.begin(),
				editor.hue_shades.end(),
				[target_luminance](const HueShade& lhs, const HueShade& rhs)
				{
					return std::fabs(lhs.luminance - target_luminance) <
						std::fabs(rhs.luminance - target_luminance);
				}
			);
			return closest->packed;
		}

		bool DrawVerticalHueBar(PaletteColourEditorState& editor)
		{
			constexpr float bar_width = 32.0f;
			constexpr float bar_height = 330.0f;
			const ImVec2 bar_size{ bar_width, bar_height };

			ImGui::InvisibleButton("##vertical_hue_bar", bar_size);
			const ImVec2 minimum = ImGui::GetItemRectMin();
			const ImVec2 maximum = ImGui::GetItemRectMax();
			ImDrawList* draw_list = ImGui::GetWindowDrawList();

			for (int segment = 0; segment < 6; ++segment)
			{
				const float start_hue = static_cast<float>(segment) / 6.0f;
				const float end_hue = static_cast<float>(segment + 1) / 6.0f;
				const ImVec4 start_colour = HuePreviewColour(start_hue);
				const ImVec4 end_colour = HuePreviewColour(end_hue >= 1.0f ? 0.0f : end_hue);
				const ImU32 start_packed = ImGui::ColorConvertFloat4ToU32(start_colour);
				const ImU32 end_packed = ImGui::ColorConvertFloat4ToU32(end_colour);
				const float y0 = minimum.y + (bar_height * static_cast<float>(segment) / 6.0f);
				const float y1 = minimum.y + (bar_height * static_cast<float>(segment + 1) / 6.0f);
				draw_list->AddRectFilledMultiColor(
					ImVec2{ minimum.x, y0 },
					ImVec2{ maximum.x, y1 },
					start_packed,
					start_packed,
					end_packed,
					end_packed
				);
			}

			draw_list->AddRect(
				minimum,
				maximum,
				ImGui::GetColorU32(ImGuiCol_Border),
				0.0f,
				0,
				1.0f
			);

			bool changed = false;
			if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				const float previous_hue = editor.selected_hue;
				const float mouse_y = ImGui::GetIO().MousePos.y;
				editor.selected_hue = std::clamp(
					(mouse_y - minimum.y) / bar_height,
					0.0f,
					0.999999f
				);
				changed = std::fabs(previous_hue - editor.selected_hue) > 0.0001f;
				if (changed)
				{
					editor.generated_hue = -1.0f;
				}
			}

			const float marker_y = minimum.y + (editor.selected_hue * bar_height);
			draw_list->AddLine(
				ImVec2{ minimum.x - 3.0f, marker_y },
				ImVec2{ maximum.x + 3.0f, marker_y },
				IM_COL32(0, 0, 0, 255),
				4.0f
			);
			draw_list->AddLine(
				ImVec2{ minimum.x - 3.0f, marker_y },
				ImVec2{ maximum.x + 3.0f, marker_y },
				IM_COL32(255, 255, 255, 255),
				2.0f
			);

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("Hue: %.0f degrees", editor.selected_hue * 360.0f);
				ImGui::EndTooltip();
			}

			return changed;
		}

		void DrawShadeGrid(
			PaletteColourEditorState& editor,
			const std::vector<HueShade>& shades,
			const char* id_prefix
		)
		{
			constexpr int columns = 8;
			constexpr float swatch_size = 34.0f;
			for (int index = 0; index < static_cast<int>(shades.size()); ++index)
			{
				const Uint16 packed = shades[index].packed;
				const bool is_selected = packed == editor.pending_packed;
				ImGui::PushID(id_prefix);
				ImGui::PushID(index);
				if (is_selected)
				{
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
				}

				if (ImGui::ColorButton(
					"##shade",
					PackedToImVec4(packed),
					ImGuiColorEditFlags_NoPicker |
						ImGuiColorEditFlags_NoTooltip |
						ImGuiColorEditFlags_NoDragDrop,
					ImVec2{ swatch_size, swatch_size }
				))
				{
					SetPendingPacked(editor, packed);
				}

				if (is_selected)
				{
					ImGui::PopStyleVar();
				}

				if (ImGui::IsItemHovered())
				{
					const rom::Colour colour = rom::Swatch{ packed }.GetUnpacked();
					ImGui::BeginTooltip();
					ImGui::Text(
						"RGB: %u, %u, %u",
						static_cast<unsigned int>(colour.r),
						static_cast<unsigned int>(colour.g),
						static_cast<unsigned int>(colour.b)
					);
					ImGui::Text("ROM value: 0x%04X", packed);
					ImGui::EndTooltip();
				}

				if ((index + 1) % columns != 0)
				{
					ImGui::SameLine();
				}
				ImGui::PopID();
				ImGui::PopID();
			}
		}

		void DrawHueMode(PaletteColourEditorState& editor)
		{
			RefreshHueShades(editor);
			if (ImGui::BeginTable(
				"hue_bar_and_shades",
				2,
				ImGuiTableFlags_SizingStretchProp
			))
			{
				ImGui::TableSetupColumn("Hue", ImGuiTableColumnFlags_WidthFixed, 80.0f);
				ImGui::TableSetupColumn("Shades", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted("Hue");
				const float previous_luminance = PackedLuminance(editor.pending_packed);
				if (DrawVerticalHueBar(editor))
				{
					SetPendingPacked(
						editor,
						FindShadeAtLuminance(editor, previous_luminance)
					);
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted("Mega Drive shades");
				ImGui::TextDisabled("Darkest to lightest");
				ImGui::Spacing();
				DrawShadeGrid(editor, editor.hue_shades, "hue");
				ImGui::EndTable();
			}
		}

		void DrawGrayscaleMode(PaletteColourEditorState& editor)
		{
			const std::vector<HueShade> shades = BuildGrayscaleShades();
			ImGui::TextUnformatted("Grayscale");
			ImGui::TextDisabled("Darkest to lightest");
			ImGui::Spacing();
			DrawShadeGrid(editor, shades, "grayscale");
		}

		void DrawRGBConversionMode(PaletteColourEditorState& editor)
		{
			ImGui::TextUnformatted("RGB values");
			ImGui::Spacing();

			ImGui::SetNextItemWidth(150.0f);
			ImGui::InputInt("Red##rgb_conversion", &editor.rgb_red, 1, 16);
			ImGui::SetNextItemWidth(150.0f);
			ImGui::InputInt("Green##rgb_conversion", &editor.rgb_green, 1, 16);
			ImGui::SetNextItemWidth(150.0f);
			ImGui::InputInt("Blue##rgb_conversion", &editor.rgb_blue, 1, 16);

			editor.rgb_red = std::clamp(editor.rgb_red, 0, 255);
			editor.rgb_green = std::clamp(editor.rgb_green, 0, 255);
			editor.rgb_blue = std::clamp(editor.rgb_blue, 0, 255);

			const ImVec4 input_colour{
				static_cast<float>(editor.rgb_red) / 255.0f,
				static_cast<float>(editor.rgb_green) / 255.0f,
				static_cast<float>(editor.rgb_blue) / 255.0f,
				1.0f
			};
			ImGui::TextUnformatted("Input");
			ImGui::ColorButton(
				"##rgb_input_preview",
				input_colour,
				ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip,
				ImVec2{ 150.0f, 54.0f }
			);

			ImGui::Spacing();
			if (ImGui::Button("Convert", ImVec2{ 190.0f, 0.0f }))
			{
				const Uint16 converted = rom::Swatch::Pack(
					static_cast<float>(editor.rgb_red) / 255.0f,
					static_cast<float>(editor.rgb_green) / 255.0f,
					static_cast<float>(editor.rgb_blue) / 255.0f
				);
				editor.pending_packed = converted;
				SetHueFromPacked(editor, converted);
			}
		}

		bool ReadPackedColour(
			const std::filesystem::path& path,
			const Uint32 offset,
			Uint16& packed
		)
		{
			if (path.empty())
			{
				return false;
			}

			std::ifstream input{ path, std::ios::binary };
			if (!input)
			{
				return false;
			}

			input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
			unsigned char bytes[2]{};
			if (!input.read(reinterpret_cast<char*>(bytes), sizeof(bytes)))
			{
				return false;
			}

			packed = static_cast<Uint16>(
				(static_cast<Uint16>(bytes[0]) << 8U) |
				static_cast<Uint16>(bytes[1])
			);
			return true;
		}

		void UpdateMegaDriveChecksum(rom::SpinballROM& rom)
		{
			if (rom.m_buffer.size() < 0x190U)
			{
				return;
			}

			Uint32 checksum = 0U;
			for (std::size_t offset = 0x200U;
				offset < rom.m_buffer.size();
				offset += 2U)
			{
				Uint16 word = static_cast<Uint16>(rom.m_buffer[offset]) << 8U;
				if (offset + 1U < rom.m_buffer.size())
				{
					word = static_cast<Uint16>(word | rom.m_buffer[offset + 1U]);
				}
				checksum = (checksum + word) & 0xFFFFU;
			}

			rom.m_buffer[0x18EU] = static_cast<Uint8>((checksum >> 8U) & 0xFFU);
			rom.m_buffer[0x18FU] = static_cast<Uint8>(checksum & 0xFFU);
		}

		bool VerifyWorkingROMWrite(
			const std::filesystem::path& path,
			const Uint32 colour_offset,
			const Uint16 expected_colour,
			const Uint16 expected_checksum
		)
		{
			Uint16 saved_colour = 0U;
			Uint16 saved_checksum = 0U;
			return ReadPackedColour(path, colour_offset, saved_colour) &&
				ReadPackedColour(path, 0x18EU, saved_checksum) &&
				saved_colour == expected_colour &&
				saved_checksum == expected_checksum;
		}

		void OpenColourEditor(
			const int palette_index,
			const int swatch_index,
			const Uint16 packed
		)
		{
			g_colour_editor.palette_index = palette_index;
			g_colour_editor.swatch_index = swatch_index;
			g_colour_editor.reference_packed = packed;
			g_colour_editor.current_packed = packed;
			g_colour_editor.pending_packed = packed;
			g_colour_editor.reference_loaded = false;
			g_colour_editor.reference_available = false;
			g_colour_editor.selected_hue = 0.0f;
			g_colour_editor.generated_hue = -1.0f;
			g_colour_editor.mode = ColourSelectionMode::Hue;
			g_colour_editor.hue_shades.clear();
			SyncRGBInputsFromPacked(g_colour_editor);
			SetHueFromPacked(g_colour_editor, packed);
			ImGui::OpenPopup(kColourEditorPopup);
		}

		bool DrawColourEditorPopup(
			EditorUI& owning_ui,
			std::vector<std::shared_ptr<rom::Palette>>& palettes,
			std::string& status
		)
		{
			bool palette_changed = false;
			ImGui::SetNextWindowSize(ImVec2{ 720.0f, 650.0f }, ImGuiCond_Appearing);

			if (!ImGui::BeginPopupModal(
				kColourEditorPopup,
				nullptr,
				ImGuiWindowFlags_NoSavedSettings
			))
			{
				return false;
			}

			const bool valid_selection =
				g_colour_editor.palette_index >= 0 &&
				g_colour_editor.palette_index < static_cast<int>(palettes.size()) &&
				palettes[g_colour_editor.palette_index] &&
				g_colour_editor.swatch_index >= 0 &&
				g_colour_editor.swatch_index < static_cast<int>(rom::Palette::s_swatches_per_palette);

			if (!valid_selection)
			{
				ImGui::TextDisabled("The selected palette colour is no longer available.");
				if (ImGui::Button("Close"))
				{
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
				return false;
			}

			rom::Palette& palette = *palettes[g_colour_editor.palette_index];
			rom::Swatch& swatch = palette.palette_swatches[g_colour_editor.swatch_index];

			const PaletteGroup palette_group = GetPaletteGroup(palette.offset);
			if (palette_group == PaletteGroup::Level ||
				palette_group == PaletteGroup::Other)
			{
				ImGui::Text(
					"Palette %02X - Colour %02X",
					g_colour_editor.palette_index,
					g_colour_editor.swatch_index
				);
			}
			else
			{
				ImGui::Text(
					"%s - Line %u - Colour %02X",
					PaletteGroupName(palette_group),
					static_cast<unsigned int>(PaletteLineNumber(palette, palette_group)),
					g_colour_editor.swatch_index
				);
			}
			ImGui::TextDisabled("ROM palette address: 0x%06X", palette.offset);
			ImGui::Separator();

			const Uint32 swatch_rom_offset = palette.offset +
				(static_cast<Uint32>(g_colour_editor.swatch_index) * sizeof(Uint16));
			if (!g_colour_editor.reference_loaded)
			{
				g_colour_editor.reference_loaded = true;
				g_colour_editor.reference_available = ReadPackedColour(
					owning_ui.GetReferenceROMPath(),
					swatch_rom_offset,
					g_colour_editor.reference_packed
				);
			}

			if (ImGui::BeginTable(
				"colour_comparison",
				3,
				ImGuiTableFlags_SizingStretchSame
			))
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted("Original ROM");
				ImGui::ColorButton(
					"##reference_colour",
					PackedToImVec4(g_colour_editor.reference_packed),
					ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip,
					ImVec2{ 150.0f, 54.0f }
				);
				if (!g_colour_editor.reference_available)
				{
					ImGui::TextDisabled("Reference unavailable");
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted("Current");
				ImGui::ColorButton(
					"##current_colour",
					PackedToImVec4(g_colour_editor.current_packed),
					ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip,
					ImVec2{ 150.0f, 54.0f }
				);

				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted("New");
				ImGui::ColorButton(
					"##pending_colour",
					PackedToImVec4(g_colour_editor.pending_packed),
					ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip,
					ImVec2{ 150.0f, 54.0f }
				);
				ImGui::EndTable();
			}

			ImGui::Spacing();
			if (ImGui::RadioButton(
				"Hue",
				g_colour_editor.mode == ColourSelectionMode::Hue
			))
			{
				g_colour_editor.mode = ColourSelectionMode::Hue;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton(
				"Grayscale",
				g_colour_editor.mode == ColourSelectionMode::Grayscale
			))
			{
				g_colour_editor.mode = ColourSelectionMode::Grayscale;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton(
				"RGB conversion",
				g_colour_editor.mode == ColourSelectionMode::RGBConversion
			))
			{
				g_colour_editor.mode = ColourSelectionMode::RGBConversion;
				SyncRGBInputsFromPacked(g_colour_editor);
			}

			ImGui::Separator();
			switch (g_colour_editor.mode)
			{
				case ColourSelectionMode::Hue:
					DrawHueMode(g_colour_editor);
					break;
				case ColourSelectionMode::Grayscale:
					DrawGrayscaleMode(g_colour_editor);
					break;
				case ColourSelectionMode::RGBConversion:
					DrawRGBConversionMode(g_colour_editor);
					break;
			}

			const rom::Swatch pending_swatch{ g_colour_editor.pending_packed };
			const rom::Colour pending_colour = pending_swatch.GetUnpacked();
			ImGui::Separator();
			ImGui::Text(
				"RGB: %u, %u, %u    ROM value: 0x%04X",
				static_cast<unsigned int>(pending_colour.r),
				static_cast<unsigned int>(pending_colour.g),
				static_cast<unsigned int>(pending_colour.b),
				g_colour_editor.pending_packed
			);

			ImGui::Spacing();
			if (ImGui::Button("Apply", ImVec2{ 110.0f, 0.0f }))
			{
				rom::SpinballROM& working_rom = owning_ui.GetROM();
				const std::filesystem::path working_path = owning_ui.GetWorkingROMPath();
				if (working_path.empty())
				{
					status = "Could not save palette colour: no working ROM path is available.";
				}
				else if (swatch_rom_offset > working_rom.m_buffer.size() ||
					sizeof(Uint16) > working_rom.m_buffer.size() - swatch_rom_offset)
				{
					status = "Could not save palette colour: invalid ROM offset.";
				}
				else
				{
					const Uint16 previous_packed = working_rom.ReadUint16(swatch_rom_offset);
					const Uint16 previous_checksum = working_rom.ReadUint16(0x18EU);

					working_rom.WriteUint16(
						swatch_rom_offset,
						g_colour_editor.pending_packed
					);
					UpdateMegaDriveChecksum(working_rom);

					// Save explicitly to the ROM copy managed by EditorUI. This avoids
					// silently writing to a stale path if the ROM object was reloaded.
					working_rom.m_filepath = working_path;
					working_rom.SaveROM();

					const Uint16 expected_checksum = working_rom.ReadUint16(0x18EU);
					if (!VerifyWorkingROMWrite(
						working_path,
						swatch_rom_offset,
						g_colour_editor.pending_packed,
						expected_checksum
					))
					{
						working_rom.WriteUint16(swatch_rom_offset, previous_packed);
						working_rom.WriteUint16(0x18EU, previous_checksum);
						working_rom.SaveROM();
						status = "Could not verify the palette colour in rom_export; the previous value was restored.";
					}
					else
					{
						swatch.packed_value = g_colour_editor.pending_packed;
						for (std::shared_ptr<rom::Palette>& loaded_palette : working_rom.m_palettes)
						{
							if (loaded_palette && loaded_palette->offset == palette.offset)
							{
								loaded_palette->palette_swatches[g_colour_editor.swatch_index].packed_value =
									g_colour_editor.pending_packed;
							}
						}
						g_colour_editor.current_packed = g_colour_editor.pending_packed;
						palette_changed = previous_packed != g_colour_editor.pending_packed;
						status = palette_changed
							? "Palette colour saved and verified in rom_export."
							: "Palette colour is already present in rom_export.";
						ImGui::CloseCurrentPopup();
					}
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Reset", ImVec2{ 110.0f, 0.0f }))
			{
				SetPendingPacked(g_colour_editor, g_colour_editor.current_packed);
				SetHueFromPacked(g_colour_editor, g_colour_editor.pending_packed);
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2{ 110.0f, 0.0f }))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
			return palette_changed;
		}
	}

	bool DrawPaletteSelectorWithPreview(int& palette_index, const EditorUI& owning_ui)
	{
		const bool selection_changed = DrawPaletteSelector(palette_index, owning_ui.GetPalettes());
		ImGui::SameLine();
		const rom::Palette& palette = *owning_ui.GetPalettes().at(palette_index);
		DrawPaletteName(palette, palette_index);
		ImGui::SameLine();
		ImGui::Dummy(ImVec2{ 0,0 });
		DrawPaletteSwatchPreview(palette);

		return selection_changed;
	}

	void DrawPaletteName(const rom::Palette& palette, int palette_index)
	{
		const PaletteGroup group = GetPaletteGroup(palette.offset);
		if (group == PaletteGroup::Level || group == PaletteGroup::Other)
		{
			ImGui::Text(
				"Palette %02X (ROM 0x%06X)",
				palette_index,
				palette.offset
			);
			return;
		}

		ImGui::Text(
			"Line %u (ROM 0x%06X)",
			static_cast<unsigned int>(PaletteLineNumber(palette, group)),
			palette.offset
		);
	}

	bool DrawPaletteSwatchEditor(rom::Palette& palette, const int palette_index)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 4.0f, 4.0f });

		for (int swatch_index = 0;
			swatch_index < static_cast<int>(palette.palette_swatches.size());
			++swatch_index)
		{
			rom::Swatch& swatch = palette.palette_swatches[swatch_index];
			char swatch_name[64];
			std::snprintf(
				swatch_name,
				sizeof(swatch_name),
				"##palette_%d_swatch_%d",
				palette_index,
				swatch_index
			);

			ImGui::SameLine();
			const bool is_selected =
				g_colour_editor.palette_index == palette_index &&
				g_colour_editor.swatch_index == swatch_index;
			if (is_selected)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
			}

			const ImColor colour = swatch.AsImColor();
			if (ImGui::ColorButton(
				swatch_name,
				colour.Value,
				ImGuiColorEditFlags_NoPicker |
				ImGuiColorEditFlags_NoTooltip,
				ImVec2{ 28.0f, 28.0f }
			))
			{
				OpenColourEditor(
					palette_index,
					swatch_index,
					swatch.packed_value
				);
			}

			if (is_selected)
			{
				ImGui::PopStyleVar();
			}

			if (ImGui::IsItemHovered())
			{
				const rom::Colour unpacked = swatch.GetUnpacked();
				ImGui::BeginTooltip();
				ImGui::Text("Colour %02X", swatch_index);
				ImGui::Text(
					"RGB: %u, %u, %u",
					static_cast<unsigned int>(unpacked.r),
					static_cast<unsigned int>(unpacked.g),
					static_cast<unsigned int>(unpacked.b)
				);
				ImGui::Text("ROM value: 0x%04X", swatch.packed_value);
				ImGui::Separator();
				ImGui::TextDisabled("Click to edit this colour.");
				ImGui::EndTooltip();
			}
		}

		ImGui::PopStyleVar();
		return false;
	}

	void DrawPaletteSwatchPreview(const rom::Palette& palette)
	{
		ImGui::NewLine();
		ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, 0);
		for (const rom::Swatch& swatch : palette.palette_swatches)
		{
			char swatch_name[64];
			std::snprintf(
				swatch_name,
				sizeof(swatch_name),
				"0x%02X###swatch_%p",
				swatch.packed_value,
				static_cast<const void*>(&swatch)
			);

			ImColor col = swatch.AsImColor();
			ImGui::SameLine();
			ImGui::ColorButton(swatch_name, col.Value);
		}
		ImGui::PopStyleVar();
	}

	bool DrawPaletteSelector(int& chosen_palette, const std::vector<std::shared_ptr<rom::Palette>>& palettes)
	{
		ImGui::SetNextItemWidth(256);
		const bool selection_changed = ImGui::SliderInt("###Palette Index", &chosen_palette, 0, static_cast<int>(palettes.size() - 1));
		return selection_changed;
	}

	bool DrawPaletteLineSelector(int& chosen_palette_line, const rom::PaletteSet& palette_set)
	{
		ImGui::SetNextItemWidth(256);
		const bool selection_changed = ImGui::SliderInt("###Palette Line", &chosen_palette_line, 0, static_cast<int>(palette_set.palette_lines.size() - 1));
		return selection_changed;
	}

	void EditorPaletteViewer::Update()
	{
		if (m_visible == false)
		{
			return;
		}

		auto& palettes = const_cast<std::vector<std::shared_ptr<rom::Palette>>&>(
			m_owning_ui.GetPalettes()
		);

		if (ImGui::Begin("Palettes", &m_visible))
		{
			if (palettes.empty())
			{
				ImGui::TextDisabled("No palette is loaded.");
				ImGui::End();
				return;
			}

			if (!m_status.empty())
			{
				ImGui::TextUnformatted(m_status.c_str());
			}

			ImGui::Separator();
			ImGui::TextDisabled(
				"Click a colour square to edit it. All values are converted to "
				"Mega Drive-compatible levels."
			);
			ImGui::Spacing();

			bool palette_changed = false;
			if (ImGui::BeginChild(
				"swatch_list",
				ImVec2{ 0,0 },
				ImGuiChildFlags_None,
				ImGuiWindowFlags_HorizontalScrollbar
			))
			{
				int palette_index = 0;
				PaletteGroup previous_group = PaletteGroup::Other;
				bool has_previous_group = false;
				for (std::shared_ptr<rom::Palette>& palette : palettes)
				{
					if (!palette)
					{
						++palette_index;
						continue;
					}

					const PaletteGroup current_group = GetPaletteGroup(palette->offset);
					if (!has_previous_group || current_group != previous_group)
					{
						ImGui::SeparatorText(PaletteGroupName(current_group));
						previous_group = current_group;
						has_previous_group = true;
					}

					DrawPaletteName(*palette, palette_index);
					palette_changed |= DrawPaletteSwatchEditor(
						*palette,
						palette_index
					);
					++palette_index;
				}

				palette_changed |= DrawColourEditorPopup(m_owning_ui, palettes, m_status);
			}
			ImGui::EndChild();

			if (palette_changed)
			{
				m_owning_ui.NotifyPaletteChanged();
			}
		}
		ImGui::End();
	}

}
