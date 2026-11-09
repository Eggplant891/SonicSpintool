#include "ui/ui_game_object_metadata.h"

#include "ui/ui_editor.h"
#include "rom/metadata/rom_metadata.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>

namespace
{
	constexpr std::array<const char*, 4> level_names{ "Toxic Caves", "Lava Powerhouse", "The Machine", "Showdown" };

	constexpr int num_flag_bits = 16;
	constexpr Uint16 flip_flags_mask = 0x6000; // Already shown as Flip X and Flip Y

	using TypeCounts = std::array<int, std::numeric_limits<Uint8>::max() + 1>;

	TypeCounts CountTypesInLevel(const spintool::rom::SpinballROM& rom, const int level_index)
	{
		TypeCounts type_counts{};

		const spintool::rom::LevelDataOffsets level_offsets{ level_index };
		for (Uint32 i = 0; i < level_offsets.object_instances.count; ++i)
		{
			const Uint32 obj_offset = level_offsets.object_instances.offset + (i * spintool::rom::GameObjectDefinition::s_size_on_rom);
			const Uint8 instance_id = rom.ReadUint8(obj_offset + 1);
			if (instance_id != 0)
			{
				++type_counts[rom.ReadUint8(obj_offset)];
			}
		}

		return type_counts;
	}

	bool IsFlagBitNamed(const spintool::metadata::GameObjectMetadata& obj_metadata, const int bit)
	{
		return std::any_of(std::begin(obj_metadata.flags), std::end(obj_metadata.flags), [bit](const spintool::metadata::GameObjectFlagMetadata& flag)
			{
				return flag.bit == bit;
			});
	}

	void SortFlags(spintool::metadata::GameObjectMetadata& obj_metadata)
	{
		std::sort(std::begin(obj_metadata.flags), std::end(obj_metadata.flags), [](const spintool::metadata::GameObjectFlagMetadata& lhs, const spintool::metadata::GameObjectFlagMetadata& rhs)
			{
				return lhs.bit < rhs.bit;
			});
	}
}

namespace spintool
{
	bool DrawGameObjectMetadataEditor(metadata::GameObjectMetadata& obj_metadata)
	{
		bool should_save = false;

		ImGui::InputText("Name", &obj_metadata.type_name);
		should_save |= ImGui::IsItemDeactivatedAfterEdit();

		ImGui::InputTextMultiline("Notes", &obj_metadata.notes, ImVec2{ 0.0f, ImGui::GetTextLineHeight() * 5.0f });
		should_save |= ImGui::IsItemDeactivatedAfterEdit();

		ImGui::InputTextWithHint("Unk 1 Name", "Unk 1", &obj_metadata.unk_1_name);
		should_save |= ImGui::IsItemDeactivatedAfterEdit();

		ImGui::InputTextWithHint("Unk 2 Name", "Unk 2", &obj_metadata.unk_2_name);
		should_save |= ImGui::IsItemDeactivatedAfterEdit();

		ImGui::SeparatorText("Flags");
		for (size_t i = 0; i < obj_metadata.flags.size(); ++i)
		{
			metadata::GameObjectFlagMetadata& flag = obj_metadata.flags[i];

			ImGui::PushID(static_cast<int>(i));
			char bit_label[32];
			snprintf(bit_label, std::size(bit_label), "Bit %d (0x%04X)", flag.bit, 1U << flag.bit);
			ImGui::SetNextItemWidth(140.0f);
			bool bit_changed = false;
			if (ImGui::BeginCombo("##bit", bit_label))
			{
				for (int bit = 0; bit < num_flag_bits; ++bit)
				{
					snprintf(bit_label, std::size(bit_label), "Bit %d (0x%04X)", bit, 1U << bit);
					const bool is_taken = bit != flag.bit && IsFlagBitNamed(obj_metadata, bit);
					if (ImGui::Selectable(bit_label, bit == flag.bit, is_taken ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None) && bit != flag.bit)
					{
						flag.bit = static_cast<Uint8>(bit);
						bit_changed = true;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(220.0f);
			ImGui::InputTextWithHint("##name", "Flag name", &flag.name);
			should_save |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::SameLine();
			const bool remove_flag = ImGui::Button("Remove");
			ImGui::PopID();

			if (remove_flag)
			{
				obj_metadata.flags.erase(std::begin(obj_metadata.flags) + static_cast<std::ptrdiff_t>(i));
				should_save = true;
				break;
			}

			if (bit_changed)
			{
				SortFlags(obj_metadata);
				should_save = true;
				break;
			}
		}

		ImGui::BeginDisabled(obj_metadata.flags.size() >= num_flag_bits);
		if (ImGui::Button("Add Flag"))
		{
			int free_bit = 0;
			while (IsFlagBitNamed(obj_metadata, free_bit))
			{
				++free_bit;
			}

			obj_metadata.flags.emplace_back().bit = static_cast<Uint8>(free_bit);
			SortFlags(obj_metadata);
			should_save = true;
		}
		ImGui::EndDisabled();

		return should_save;
	}

	void DrawGameObjectMetadataSummary(const metadata::GameObjectMetadata* obj_metadata, const Uint16 flags)
	{
		ImGui::SeparatorText("Metadata");
		if (obj_metadata == nullptr || (obj_metadata->type_name.empty() && obj_metadata->notes.empty()))
		{
			ImGui::TextUnformatted("<No metadata>");
		}
		else
		{
			if (obj_metadata->type_name.empty() == false)
			{
				ImGui::Text("Name: %s", obj_metadata->type_name.c_str());
			}

			if (obj_metadata->notes.empty() == false)
			{
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
				ImGui::TextUnformatted(obj_metadata->notes.c_str());
				ImGui::PopTextWrapPos();
			}
		}

		bool has_drawn_header = false;
		const auto draw_flag = [&has_drawn_header](const bool is_set, const int bit, const char* name)
			{
				if (has_drawn_header == false)
				{
					ImGui::SeparatorText("Flags");
					has_drawn_header = true;
				}

				if (name != nullptr && name[0] != '\0')
				{
					ImGui::Text("[%c] %s", is_set ? 'x' : ' ', name);
				}
				else
				{
					ImGui::Text("[%c] Bit %d", is_set ? 'x' : ' ', bit);
				}
			};

		if (obj_metadata != nullptr)
		{
			for (const metadata::GameObjectFlagMetadata& flag : obj_metadata->flags)
			{
				draw_flag((flags & (1U << flag.bit)) != 0, flag.bit, flag.name.c_str());
			}
		}

		// Flags set on this object that nobody has named yet are worth showing, as they are undiscovered.
		for (int bit = 0; bit < num_flag_bits; ++bit)
		{
			const Uint16 bit_mask = static_cast<Uint16>(1U << bit);
			const bool is_named = obj_metadata != nullptr && IsFlagBitNamed(*obj_metadata, bit);
			if ((flags & bit_mask) != 0 && (flip_flags_mask & bit_mask) == 0 && is_named == false)
			{
				draw_flag(true, bit, nullptr);
			}
		}
	}

	void EditorGameObjectMetadataWindow::Update()
	{
		if (IsOpen() == false)
		{
			return;
		}

		metadata::ROMMetadata* rom_metadata = m_owning_ui.GetCurrentROMMetadata();
		if (rom_metadata == nullptr)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2{ 820.0f, 520.0f }, ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Game Object Metadata", &m_visible, ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::SetNextItemWidth(200.0f);
			if (ImGui::Combo("Level", &m_level_index, level_names.data(), static_cast<int>(level_names.size())))
			{
				m_selected_type.reset();
			}
			ImGui::SameLine();
			ImGui::Checkbox("Show all type IDs", &m_show_all_types);

			const TypeCounts type_counts = CountTypesInLevel(m_owning_ui.GetROM(), m_level_index);

			if (ImGui::BeginChild("type_list", ImVec2{ 300.0f, 0.0f }, ImGuiChildFlags_Borders))
			{
				if (ImGui::BeginTable("types", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
				{
					ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableHeadersRow();

					for (size_t type_id = 0; type_id < type_counts.size(); ++type_id)
					{
						const metadata::GameObjectMetadata* obj_metadata = rom_metadata->GetGameObjectMetadata(m_level_index, static_cast<Uint8>(type_id));
						if (m_show_all_types == false && type_counts[type_id] == 0 && obj_metadata->IsEmpty())
						{
							continue;
						}

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						char type_label[8];
						snprintf(type_label, std::size(type_label), "0x%02X", static_cast<unsigned int>(type_id));
						if (ImGui::Selectable(type_label, m_selected_type == static_cast<Uint8>(type_id), ImGuiSelectableFlags_SpanAllColumns))
						{
							m_selected_type = static_cast<Uint8>(type_id);
						}
						ImGui::TableNextColumn();
						ImGui::Text("%d", type_counts[type_id]);
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(obj_metadata->type_name.c_str());
					}
					ImGui::EndTable();
				}
			}
			ImGui::EndChild();

			ImGui::SameLine();
			if (ImGui::BeginChild("type_details", ImVec2{ 0.0f, 0.0f }, ImGuiChildFlags_Borders))
			{
				if (m_selected_type)
				{
					ImGui::Text("%s - Type 0x%02X", level_names[m_level_index], *m_selected_type);
					ImGui::Separator();
					if (DrawGameObjectMetadataEditor(*rom_metadata->GetGameObjectMetadata(m_level_index, *m_selected_type)))
					{
						m_owning_ui.SaveGameObjectMetadata();
					}
				}
				else
				{
					ImGui::TextUnformatted("Select a type ID to edit its metadata.");
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}
}
