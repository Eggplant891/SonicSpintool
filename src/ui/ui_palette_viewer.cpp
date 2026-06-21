#include "ui/ui_palette_viewer.h"

#include "SDL3/SDL_stdinc.h"
#include "ui/ui_editor.h"
#include "render.h"


namespace spintool
{
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
		ImGui::Text("Palette %02X (0x%04X)", palette_index, palette.offset);
	}

	bool DrawPaletteSwatchEditor(rom::Palette& palette)
	{
		bool palette_changed = false;
		ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, 0);
		for (rom::Swatch& swatch : palette.palette_swatches)
		{
			char swatch_name[64];
			sprintf(swatch_name, "0x%04X###swatch_%p", swatch.packed_value, &swatch);

			ImColor col = swatch.AsImColor();
			ImGui::SameLine();
			if (ImGui::ColorEdit3(
				swatch_name,
				&col.Value.x,
				ImGuiColorEditFlags_Uint8 |
				ImGuiColorEditFlags_NoLabel |
				ImGuiColorEditFlags_NoSidePreview |
				ImGuiColorEditFlags_NoInputs
			))
			{
				const Uint16 packed = rom::Swatch::Pack(
					col.Value.x, col.Value.y, col.Value.z
				);
				if (packed != swatch.packed_value)
				{
					swatch.packed_value = packed;
					palette_changed = true;
				}
			}
		}
		ImGui::PopStyleVar();
		return palette_changed;
	}

	void DrawPaletteSwatchPreview(const rom::Palette& palette)
	{
		ImGui::NewLine();
		ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, 0);
		for (const rom::Swatch& swatch : palette.palette_swatches)
		{
			char swatch_name[64];
			sprintf(swatch_name, "0x%02X###swatch_%p", swatch.packed_value, &swatch);

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

			if (ImGui::Button("Save Changes"))
			{
				rom::SpinballROM& rom = m_owning_ui.GetROM();
				bool valid = true;

				// Palettes are not all contiguous in the ROM. Always use each
				// palette's own address instead of writing from the first palette
				// through the gaps between the level and cutscene palette tables.
				for (const std::shared_ptr<rom::Palette>& palette : palettes)
				{
					if (!palette ||
						palette->offset > rom.m_buffer.size() ||
						rom::Palette::s_palette_size_on_rom >
							rom.m_buffer.size() - static_cast<std::size_t>(palette->offset))
					{
						valid = false;
						break;
					}
				}

				if (valid)
				{
					for (const std::shared_ptr<rom::Palette>& palette : palettes)
					{
						Uint32 write_offset = palette->offset;
						for (const rom::Swatch& swatch : palette->palette_swatches)
						{
							write_offset = rom.WriteUint16(
								write_offset, swatch.packed_value
							);
						}
					}
					rom.SaveROM();
					m_status = "Palette changes saved to the working ROM.";
				}
				else
				{
					m_status = "Could not save palette changes: invalid ROM offset.";
				}
			}

			if (!m_status.empty())
			{
				ImGui::SameLine();
				ImGui::TextUnformatted(m_status.c_str());
			}

			ImGui::Separator();
			bool palette_changed = false;
			if (ImGui::BeginChild("swatch_list"))
			{
				int palette_index = 0;
				for (std::shared_ptr<rom::Palette>& palette : palettes)
				{
					if (!palette)
					{
						++palette_index;
						continue;
					}

					DrawPaletteName(*palette, palette_index);
					ImGui::SameLine();
					ImGui::Dummy(ImVec2{ 0,0 });
					palette_changed |= DrawPaletteSwatchEditor(*palette);
					++palette_index;
				}
			}
			ImGui::EndChild();

			if (palette_changed)
			{
				m_status = "Palette modified. Click Save Changes to write it to the ROM.";
				m_owning_ui.NotifyPaletteChanged();
			}
		}
		ImGui::End();
	}

}
