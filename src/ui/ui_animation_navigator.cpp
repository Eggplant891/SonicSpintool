#include "ui/ui_animation_navigator.h"

#include "ui/ui_editor.h"
#include "ui/ui_palette.h"
#include "rom/palette.h"
#include "rom/rom_asset_definitions.h"
#include "rom/metadata/rom_metadata.h"
#include "rom/metadata/animation_palette_scraper.h"
#include <algorithm>
#include "types/rom_ptr.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace spintool
{
	namespace
	{
		// Animation callbacks that the preview simulates (USA ROM addresses; AnimCallback_* in the disassembly).
		constexpr Uint32 s_anim_callback_set_flip_x = 0x000D61AC;
		constexpr Uint32 s_anim_callback_clear_flip_x = 0x000D61B8;
		constexpr Uint32 s_anim_callback_toggle_flip_x = 0x000D61C4;
		constexpr Uint32 s_anim_callback_stop_if_count_reached = 0x000D61D2;

		const char* KnownCallbackName(const Uint32 address)
		{
			switch (address)
			{
			case 0x000D617A: return "AnimCallback_SetBBoxFromParams";
			case 0x000D6194: return "AnimCallback_ToxicCavesTeleporterTongue";
			case s_anim_callback_set_flip_x: return "AnimCallback_SetFlipX";
			case s_anim_callback_clear_flip_x: return "AnimCallback_ClearFlipX";
			case s_anim_callback_toggle_flip_x: return "AnimCallback_ToggleFlipX";
			case s_anim_callback_stop_if_count_reached: return "AnimCallback_StopIfCountReached";
			case 0x000E2F94: return "SetPlayerPositionToGameObjectPtr";
			default: return nullptr;
			}
		}

		void FormatCommandLabel(char* buffer, const size_t buffer_size, const rom::AnimationCommand& command)
		{
			const int type_value = static_cast<int>(command.command_type);
			const char* type_name = rom::AnimationCommandTypeToString(command.command_type);
			int written = 0;
			if (command.IsFrame())
			{
				written = snprintf(buffer, buffer_size, "0x%04X%s <%s>", command.command_data, command.flip_x ? " (flip X)" : "", type_name);
			}
			else
			{
				char operand[48];
				switch (command.operand_mode)
				{
				case rom::AnimationOperandMode::INLINE:
					snprintf(operand, sizeof(operand), "%d", command.inline_value);
					break;
				case rom::AnimationOperandMode::VARIABLE:
					snprintf(operand, sizeof(operand), "var 0x%02X", command.argument & 0x7F);
					break;
				case rom::AnimationOperandMode::POINTER:
					snprintf(operand, sizeof(operand), "0x%02X, ptr 0x%08X", command.argument, command.pointer);
					break;
				case rom::AnimationOperandMode::TABLE_LOOKUP:
					snprintf(operand, sizeof(operand), "table 0x%08X[var 0x%02X]", command.pointer, command.argument & 0x7F);
					break;
				}
				const char* callback_name = command.command_type == rom::AnimationCommandType::EXECUTE_AS_FUNCTION_PTR ? KnownCallbackName(command.pointer) : nullptr;
				if (callback_name != nullptr)
				{
					written = snprintf(buffer, buffer_size, "%s <%s (0x%02X)>", callback_name, type_name, type_value & 0xFF);
				}
				else
				{
					written = snprintf(buffer, buffer_size, "%s <%s (0x%02X)>", operand, type_name, type_value & 0xFF);
				}
				for (size_t i = 0; i < command.jump_targets.size() && written > 0 && static_cast<size_t>(written) < buffer_size; ++i)
				{
					written += snprintf(buffer + written, buffer_size - written, "%s0x%08X", i == 0 ? " -> " : ", ", command.jump_targets[i]);
				}
			}
			if (written > 0 && static_cast<size_t>(written) < buffer_size)
			{
				snprintf(buffer + written, buffer_size - written, "###%08X", command.rom_data.rom_offset);
			}
		}
	}

	void UIAnimationSequence::ResetAnimation()
	{
		anim_offset = { 0,0 };
		preview_loop_count = 0;
		remaining_ticks_on_frame = 0;
		ticks_per_frame = base_ticks_per_frame;
		current_command = anim_sequence->start_command_index;
		current_visual_command = anim_sequence->start_command_index;
		loop_counter = 0;
		restart_delay = 0;
		frame_hidden = false;
		object_flip_x = false;
		variables.fill(0);
	}

	Sint32 UIAnimationSequence::ReadOperand(const rom::AnimationCommand& command) const
	{
		if (command.operand_mode == rom::AnimationOperandMode::INLINE)
		{
			return command.inline_value;
		}

		// Animation_SetFrameSpriteIndex: the low 7 bits pick the variable
		const Uint16 index = command.argument & 0x7F;
		const Sint32 value = index < variables.size() ? variables[index] : 0;
		// TABLE_LOOKUP would read table[value] from the ROM; the preview treats the variable as the operand.
		return value;
	}

	void UIAnimationSequence::WriteVariable(const Uint16 index, const Sint16 value)
	{
		const Uint16 slot = index & 0x7F;
		if (slot < variables.size())
		{
			variables[slot] = value;
		}
	}

	// One game tick, following AnimObj_UpdateTemporaries / AnimObj_UpdateAnimation.
	void UIAnimationSequence::Tick()
	{
		const std::vector<rom::AnimationCommand>& commands = anim_sequence->command_sequence;
		if (commands.empty())
		{
			return;
		}

		if (restart_delay > 0)
		{
			if (--restart_delay > 0)
			{
				return;
			}
			// Restart and run the first tick now, so the first frame replaces the last one without a blank tick
			// (the animation's first command is often not a frame).
			ResetAnimation();
		}

		if (remaining_ticks_on_frame > 0)
		{
			--remaining_ticks_on_frame;
			return;
		}

		const auto go_to = [this](const Uint32 target) -> bool
		{
			if (const std::optional<size_t> index = anim_sequence->FindCommandIndex(target))
			{
				current_command = *index;
				return true;
			}
			return false;
		};
		const auto finish = [this]()
		{
			restart_delay = base_ticks_per_frame * 8;
		};

		// The game runs commands until it reaches a frame or a command that ends the tick.
		for (int guard = 0; guard < 256; ++guard)
		{
			if (current_command >= commands.size())
			{
				finish();
				return;
			}

			const rom::AnimationCommand& command = commands[current_command];
			const std::optional<size_t> next = anim_sequence->FindCommandIndex(command.rom_data.rom_offset_end);
			const auto advance = [this, &next, &finish]() -> bool
			{
				if (next.has_value() == false)
				{
					finish();
					return false;
				}
				current_command = *next;
				return true;
			};

			switch (command.command_type)
			{
			case rom::AnimationCommandType::FRAME:
			case rom::AnimationCommandType::HIDDEN_FRAME:
			case rom::AnimationCommandType::COMPUTED_FRAME:
				// The preview can't work out a computed frame's sprite, so it hides the sprite for that frame.
				frame_hidden = command.command_type != rom::AnimationCommandType::FRAME;
				current_visual_command = current_command;
				// The game's timers are bytes: ticks per frame of 0 gives 256 ticks (how AnimCallback_StopIfCountReached stops an animation).
				remaining_ticks_on_frame = (ticks_per_frame - 1) & 0xFF;
				advance();
				return;

			case rom::AnimationCommandType::GOTO:
				if (command.jump_targets.empty() || go_to(command.jump_targets.front()) == false)
				{
					finish();
					return;
				}
				if (++preview_loop_count >= preview_loop_max)
				{
					finish();
					return;
				}
				continue;

			case rom::AnimationCommandType::LOOP:
				if (++loop_counter < command.argument && command.jump_targets.empty() == false)
				{
					if (go_to(command.jump_targets.front()))
					{
						continue;
					}
				}
				loop_counter = 0;
				break;

			case rom::AnimationCommandType::SWITCH:
			{
				const Sint32 value = ReadOperand(command);
				if (value >= 1 && static_cast<size_t>(value) <= command.jump_targets.size())
				{
					if (go_to(command.jump_targets[value - 1]))
					{
						continue;
					}
				}
				break;
			}

			case rom::AnimationCommandType::HOLD_FRAME:
				remaining_ticks_on_frame = (ReadOperand(command) - 1) & 0xFF;
				advance();
				return;

			case rom::AnimationCommandType::HOLD_FRAME_RANDOM:
			{
				const Sint32 range = ReadOperand(command);
				remaining_ticks_on_frame = range > 0 ? std::rand() % range : 0;
				advance();
				return;
			}

			case rom::AnimationCommandType::PAUSE:
				// The animation stops on its last frame until the game switches animation: hold it, then restart the preview.
				finish();
				return;

			case rom::AnimationCommandType::WAIT_FOR_FLAG:
				// In the game the object waits until other code sets its wake flag. The preview waits a frame's worth of ticks and carries on.
				remaining_ticks_on_frame = std::max(base_ticks_per_frame - 1, 0);
				advance();
				return;

			case rom::AnimationCommandType::SET_ANIM_TICKS_PER_FRAME:
				ticks_per_frame = ReadOperand(command) & 0xFF;
				break;

			case rom::AnimationCommandType::CLEAR_VARIABLE:
				WriteVariable(static_cast<Uint16>(ReadOperand(command)), 0);
				break;

			case rom::AnimationCommandType::SET_VARIABLE:
				WriteVariable(static_cast<Uint16>(ReadOperand(command)), static_cast<Sint16>(command.extra_word));
				break;

			case rom::AnimationCommandType::ADD_X_OFFSET:
				anim_offset.x += ReadOperand(command);
				break;

			case rom::AnimationCommandType::ADD_Y_OFFSET:
				anim_offset.y += ReadOperand(command);
				break;

			case rom::AnimationCommandType::SET_X_OFFSET:
			{
				// An absolute level position: show it relative to the animation's first one.
				const Sint32 value = ReadOperand(command);
				anim_offset.x = value - anim_sequence->set_x_origin.value_or(value);
				break;
			}

			case rom::AnimationCommandType::SET_Y_OFFSET:
			{
				// An absolute level position: show it relative to the animation's first one.
				const Sint32 value = ReadOperand(command);
				anim_offset.y = value - anim_sequence->set_y_origin.value_or(value);
				break;
			}

			case rom::AnimationCommandType::EXECUTE_AS_FUNCTION_PTR:
				switch (command.pointer)
				{
				case s_anim_callback_set_flip_x: object_flip_x = true; break;
				case s_anim_callback_clear_flip_x: object_flip_x = false; break;
				case s_anim_callback_toggle_flip_x: object_flip_x = !object_flip_x; break;
				case s_anim_callback_stop_if_count_reached:
					// Object bytes $2F and $30 are variables $41 and $42. The game sets $42 to the frame to stop on.
					if (variables[0x41] == variables[0x42])
					{
						ticks_per_frame = 0;
						remaining_ticks_on_frame = 0;
					}
					break;
				default: break; // Other callbacks don't affect the preview
				}
				break;

			case rom::AnimationCommandType::KILL_OBJECT:
			case rom::AnimationCommandType::INVALID:
			case rom::AnimationCommandType::UNKNOWN:
				finish();
				return;

			default:
				// Audio, function calls, object creation, tile loads and speed changes don't affect the preview.
				break;
			}

			if (advance() == false)
			{
				return;
			}
		}
	}

	std::shared_ptr<const rom::Palette> EditorAnimationNavigator::ResolvePalette(const Uint32 animation_offset) const
	{
		const rom::SpinballROM& rom = m_owning_ui.GetROM();
		const metadata::AnimationMetadata defaults;
		const metadata::ROMMetadata* rom_metadata = m_owning_ui.GetCurrentROMMetadata();
		const metadata::AnimationMetadata* entry = rom_metadata != nullptr ? rom_metadata->GetAnimationMetadata(animation_offset) : nullptr;
		if (entry == nullptr)
		{
			entry = &defaults;
		}

		const auto line_of = [entry](const std::shared_ptr<rom::PaletteSet>& palette_set) -> std::shared_ptr<const rom::Palette>
		{
			if (palette_set == nullptr || entry->palette_line >= palette_set->palette_lines.size())
			{
				return nullptr;
			}
			return palette_set->palette_lines[entry->palette_line];
		};

		switch (entry->palette_set)
		{
		case metadata::AnimationPaletteSet::GLOBAL_PALETTES:
		{
			const auto& palettes = rom.GetGlobalPalettes();
			return entry->palette_line < palettes.size() ? palettes[entry->palette_line] : nullptr;
		}
		case metadata::AnimationPaletteSet::TOXIC_CAVES:
		case metadata::AnimationPaletteSet::LAVA_POWERHOUSE:
		case metadata::AnimationPaletteSet::THE_MACHINE:
		case metadata::AnimationPaletteSet::SHOWDOWN:
		{
			const int level_index = static_cast<int>(entry->palette_set) - static_cast<int>(metadata::AnimationPaletteSet::TOXIC_CAVES);
			const rom::LevelDataOffsets level_offsets{ level_index };
			return line_of(rom::PaletteSet::LoadFromROM(rom, level_offsets.palette_set));
		}
		case metadata::AnimationPaletteSet::MAIN_MENU:
			return line_of(rom.GetMainMenuPaletteSet());
		case metadata::AnimationPaletteSet::INTRO_CUTSCENE:
			return line_of(rom.GetIntroCutscenePaletteSet());
		case metadata::AnimationPaletteSet::OPTIONS_SCREEN:
			return line_of(rom.GetOptionsScreenPaletteSet());
		case metadata::AnimationPaletteSet::SEGA_LOGO:
			return line_of(rom.GetSegaLogoIntroPaletteSet());
		case metadata::AnimationPaletteSet::COUNT:
			break;
		}
		return nullptr;
	}

	void EditorAnimationNavigator::ApplyPalette(UIAnimationSequence& anim)
	{
		if (anim.anim_sequence == nullptr)
		{
			return;
		}

		// The metadata's initial ticks per frame (0 = not set: keep the preview's default).
		const metadata::ROMMetadata* tick_metadata = m_owning_ui.GetCurrentROMMetadata();
		const metadata::AnimationMetadata* tick_entry = tick_metadata != nullptr ? tick_metadata->GetAnimationMetadata(static_cast<Uint32>(anim.anim_sequence->rom_data.rom_offset)) : nullptr;

		// The metadata's sprite table (0 = the one the list was loaded with): reload the frames if it differs.
		const Uint32 wanted_sprite_table = tick_entry != nullptr && tick_entry->sprite_table != 0 ? tick_entry->sprite_table : anim.default_sprite_table;
		if (wanted_sprite_table != anim.loaded_sprite_table)
		{
			anim.anim_sequence = rom::AnimationSequence::LoadFromROM(m_owning_ui.GetROM(), static_cast<Uint32>(anim.anim_sequence->rom_data.rom_offset), wanted_sprite_table);
			anim.loaded_sprite_table = wanted_sprite_table;
			anim.ResetAnimation();
			anim.Tick();
		}
		const int wanted_ticks = tick_entry != nullptr && tick_entry->ticks_per_frame != 0 ? tick_entry->ticks_per_frame : UIAnimationSequence::s_default_ticks_per_frame;
		if (anim.base_ticks_per_frame != wanted_ticks)
		{
			anim.base_ticks_per_frame = wanted_ticks;
			anim.ResetAnimation();
			anim.Tick();
		}

		const std::shared_ptr<const rom::Palette> palette = ResolvePalette(static_cast<Uint32>(anim.anim_sequence->rom_data.rom_offset));
		if (palette == nullptr)
		{
			return;
		}

		const UIPalette ui_palette{ *palette };
		for (const rom::AnimationCommand& command : anim.anim_sequence->command_sequence)
		{
			if (command.ui_frame_sprite != nullptr)
			{
				command.ui_frame_sprite->texture = command.ui_frame_sprite->RenderTextureForPalette(ui_palette, command.flip_x);
			}
		}
	}

	bool EditorAnimationNavigator::DrawMetadataEditor(UIAnimationSequence& anim)
	{
		metadata::ROMMetadata* rom_metadata = m_owning_ui.GetCurrentROMMetadata();
		if (rom_metadata == nullptr)
		{
			ImGui::TextDisabled("No ROM metadata is loaded, so this animation can't be annotated.");
			return false;
		}

		const Uint32 animation_offset = static_cast<Uint32>(anim.anim_sequence->rom_data.rom_offset);
		const metadata::AnimationMetadata* existing = rom_metadata->GetAnimationMetadata(animation_offset);
		metadata::AnimationMetadata edited = existing != nullptr ? *existing : metadata::AnimationMetadata{};
		edited.rom_offset = animation_offset;

		bool changed = false;
		bool palette_changed = false;

		char name_buffer[128];
		snprintf(name_buffer, sizeof(name_buffer), "%s", edited.name.c_str());
		ImGui::SetNextItemWidth(256);
		ImGui::InputText("Name", name_buffer, sizeof(name_buffer));
		if (ImGui::IsItemDeactivatedAfterEdit() && edited.name != name_buffer)
		{
			edited.name = name_buffer;
			changed = true;
		}

		int palette_set_index = static_cast<int>(edited.palette_set);
		ImGui::SetNextItemWidth(256);
		if (ImGui::Combo("Palette set", &palette_set_index, metadata::s_animation_palette_set_names.data(), static_cast<int>(metadata::s_animation_palette_set_names.size())))
		{
			const auto new_set = static_cast<metadata::AnimationPaletteSet>(palette_set_index);
			// A level or screen palette set only has 4 lines.
			if (metadata::AnimationMetadata::UsesPaletteIndex(new_set) == false)
			{
				edited.palette_line = static_cast<Uint8>(edited.palette_line % rom::s_max_palettes);
			}
			edited.palette_set = new_set;
			changed = palette_changed = true;
		}

		const bool uses_index = metadata::AnimationMetadata::UsesPaletteIndex(edited.palette_set);
		const int max_line = uses_index ? std::max(static_cast<int>(m_owning_ui.GetROM().GetGlobalPalettes().size()) - 1, 0) : static_cast<int>(rom::s_max_palettes) - 1;
		int palette_line = std::min<int>(edited.palette_line, max_line);
		ImGui::SetNextItemWidth(256);
		if (ImGui::SliderInt(uses_index ? "Palette index" : "Palette line", &palette_line, 0, max_line))
		{
			edited.palette_line = static_cast<Uint8>(palette_line);
			changed = palette_changed = true;
		}

		{
			// Sprite tables the object definitions use (USA ROM addresses).
			struct KnownSpriteTable { const char* name; Uint32 offset; };
			static constexpr KnownSpriteTable s_sprite_tables[] =
			{
				{ "Toxic Caves", 0x00012B0C },
				{ "Lava Powerhouse", 0x00020666 },
				{ "The Machine", 0x0002B7A4 },
				{ "Showdown", 0x000318EE },
				{ "Shared", 0x0000E47A },
				{ "Player", 0x000013DC },
			};
			char default_label[48];
			snprintf(default_label, sizeof(default_label), "Default (0x%05X)", anim.default_sprite_table);
			const char* preview = default_label;
			for (const KnownSpriteTable& table : s_sprite_tables)
			{
				if (edited.sprite_table == table.offset)
				{
					preview = table.name;
				}
			}
			ImGui::SetNextItemWidth(256);
			if (ImGui::BeginCombo("Sprite table", preview))
			{
				if (ImGui::Selectable(default_label, edited.sprite_table == 0))
				{
					edited.sprite_table = 0;
					changed = palette_changed = true;
				}
				for (const KnownSpriteTable& table : s_sprite_tables)
				{
					char label[48];
					snprintf(label, sizeof(label), "%s (0x%05X)", table.name, table.offset);
					if (ImGui::Selectable(label, edited.sprite_table == table.offset))
					{
						edited.sprite_table = table.offset;
						changed = palette_changed = true;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SetItemTooltip("The sprite table the frames index: the object definition's sprite table.\nAnimations in a level's table can also use the shared table.");
		}

		int ticks = edited.ticks_per_frame;
		ImGui::SetNextItemWidth(256);
		if (ImGui::SliderInt("Ticks per frame", &ticks, 0, 255, ticks == 0 ? "Not set (6)" : "%d"))
		{
			edited.ticks_per_frame = static_cast<Uint8>(ticks);
			changed = palette_changed = true;
		}
		ImGui::SetItemTooltip("The game's initial ticks per frame for the object that plays this animation\n(its definition's frame_time_ticks). A \"Set anim ticks per frame\" command can change it.");

		if (changed)
		{
			if (edited.IsEmpty())
			{
				rom_metadata->animation_metadata.erase(animation_offset);
			}
			else
			{
				rom_metadata->GetOrCreateAnimationMetadata(animation_offset) = edited;
			}
		}
		if (palette_changed)
		{
			ApplyPalette(anim);
		}
		return changed;
	}

	void EditorAnimationNavigator::FillPalettesFromLevelObjects()
	{
		metadata::ROMMetadata* rom_metadata = m_owning_ui.GetCurrentROMMetadata();
		if (rom_metadata == nullptr)
		{
			m_metadata_status = "No ROM metadata is loaded.";
			return;
		}

		const std::map<Uint32, metadata::ScrapedAnimationUsage> scraped = metadata::ScrapeAnimationPalettes(m_owning_ui.GetROM());
		int filled = 0;
		int already_set = 0;
		int conflicts = 0;
		int ticks_filled = 0;
		int sprite_tables_filled = 0;
		for (const auto& [animation_offset, usage] : scraped)
		{
			// Sprite table: the same rule.
			if (usage.sprite_tables.size() == 1)
			{
				const metadata::AnimationMetadata* existing = rom_metadata->GetAnimationMetadata(animation_offset);
				if (existing == nullptr || existing->sprite_table == 0)
				{
					rom_metadata->GetOrCreateAnimationMetadata(animation_offset).sprite_table = usage.MostCommonSpriteTable();
					++sprite_tables_filled;
				}
			}

			// Tick rate: fill it when every definition agrees and the user hasn't set one.
			if (usage.ticks_per_frame.size() == 1)
			{
				const metadata::AnimationMetadata* existing = rom_metadata->GetAnimationMetadata(animation_offset);
				if (existing == nullptr || existing->ticks_per_frame == 0)
				{
					rom_metadata->GetOrCreateAnimationMetadata(animation_offset).ticks_per_frame = usage.MostCommonTicksPerFrame();
					++ticks_filled;
				}
			}

			if (usage.palettes.size() != 1)
			{
				++conflicts; // Used with different palettes (e.g. shared between levels): leave it for the user
				continue;
			}

			const metadata::AnimationMetadata* existing = rom_metadata->GetAnimationMetadata(animation_offset);
			const bool has_user_palette = existing != nullptr
				&& (existing->palette_set != metadata::AnimationMetadata::s_default_palette_set || existing->palette_line != metadata::AnimationMetadata::s_default_palette_line);
			if (has_user_palette)
			{
				++already_set;
				continue;
			}

			const metadata::ScrapedAnimationPalette palette = usage.MostCommon();
			metadata::AnimationMetadata& entry = rom_metadata->GetOrCreateAnimationMetadata(animation_offset);
			entry.palette_set = palette.palette_set;
			entry.palette_line = palette.palette_line;
			++filled;
		}

		if (filled > 0 || ticks_filled > 0 || sprite_tables_filled > 0)
		{
			m_owning_ui.SaveAnimationMetadata();
			for (UIAnimationSequence& anim : m_animations)
			{
				ApplyPalette(anim);
			}
		}

		char status[384];
		snprintf(status, sizeof(status), "Filled the palette for %d animation(s), the tick rate for %d and the sprite table for %d from level objects. Kept %d palette(s) you had already set; skipped %d used with more than one palette.", filled, ticks_filled, sprite_tables_filled, already_set, conflicts);
		m_metadata_status = status;
	}

	void EditorAnimationNavigator::LoadPlayerAnimationTables()
	{
		const rom::SpinballROM& rom = m_owning_ui.GetROM();

		static Uint32 anims_offset = 0x6FE;
		static Uint32 num_anims = 58;

		m_animations.reserve(num_anims);
		for (Uint32 i = 0; i < num_anims; ++i)
		{
			UIAnimationSequence sequence = { rom::AnimationSequence::LoadFromROM(rom, rom.ReadUint32(anims_offset + (i * 4)), 0x13DC), 0x13DC, 0x13DC };
			sequence.ResetAnimation();
			ApplyPalette(sequence);
			sequence.Tick(); // Pick the first frame, so there is something to show even at preview speed 0
			m_animations.emplace_back(std::move(sequence));
		}
	}

	void EditorAnimationNavigator::LoadAnimationsScrapedFromAnimObjects()
	{
		static Uint32 anim_objs_start = 0x000C0B14;
		static Uint32 anim_objs_end = 0x000C13F0;
		static Uint32 num_anims = (anim_objs_end - anim_objs_start) / rom::AnimObjectDefinition::s_size_on_rom;

		m_animations.reserve(num_anims);

		for (Uint32 current_anim_obj = anim_objs_start; current_anim_obj != anim_objs_end; current_anim_obj += rom::AnimObjectDefinition::s_size_on_rom)
		{
			const rom::AnimObjectDefinition anim_obj = rom::AnimObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), current_anim_obj);
			if (anim_obj.starting_animation != 0)
			{
				const Uint32 sprite_table = m_owning_ui.GetROM().ReadUint32(anim_obj.sprite_table);
				UIAnimationSequence sequence = { rom::AnimationSequence::LoadFromROM(m_owning_ui.GetROM(), anim_obj.starting_animation, sprite_table), sprite_table, sprite_table };
				sequence.ResetAnimation();
				ApplyPalette(sequence);
				sequence.Tick(); // Pick the first frame, so there is something to show even at preview speed 0
			m_animations.emplace_back(std::move(sequence));
			}
		}
	}

	void EditorAnimationNavigator::LoadLevelAnimations(int level_index)
	{
		const rom::LevelDataOffsets level_offsets{ level_index };

		const rom::SpinballROM& rom = m_owning_ui.GetROM();

		const Uint32 level_anims_ptr_table_offset = level_offsets.animations;
		const Uint32 anims_offset = rom.ReadUint32(level_anims_ptr_table_offset); // 0x0003909E;
		const Uint32 sprite_table_ptr = level_offsets.sprite_table;
		const Uint32 num_anims = level_offsets.animation_count;

		m_animations.reserve(num_anims);
		for (Uint32 i = 0; i < num_anims; ++i)
		{
			UIAnimationSequence sequence = { rom::AnimationSequence::LoadFromROM(rom, rom.ReadUint32(anims_offset + (i * 4)), sprite_table_ptr), sprite_table_ptr, sprite_table_ptr };
			sequence.ResetAnimation();
			ApplyPalette(sequence);
			sequence.Tick(); // Pick the first frame, so there is something to show even at preview speed 0
			m_animations.emplace_back(std::move(sequence));
		}
	}

	void EditorAnimationNavigator::Update()
	{
		if (IsOpen() == false)
		{
			return;
		}

		// Run as many game ticks as the elapsed time covers, scaled by the preview speed.
		const auto now = std::chrono::steady_clock::now();
		if (m_last_update_time == std::chrono::steady_clock::time_point{})
		{
			m_last_update_time = now;
		}
		const double elapsed_seconds = std::chrono::duration<double>(now - m_last_update_time).count();
		m_last_update_time = now;
		constexpr double s_game_ticks_per_second = 60.0;
		constexpr int s_max_ticks_per_update = 32;
		m_tick_accumulator = std::min(m_tick_accumulator + elapsed_seconds * s_game_ticks_per_second * m_preview_speed, static_cast<double>(s_max_ticks_per_update));
		const int ticks_to_run = static_cast<int>(m_tick_accumulator);
		m_tick_accumulator -= ticks_to_run;

		ImGui::SetNextWindowSize(ImVec2(800, 800), ImGuiCond_Appearing);
		if (ImGui::Begin("Animation Navigator", &m_visible, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar))
		{
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("Load"))
				{
					if (ImGui::MenuItem("Player"))
					{
						m_animations.clear();
						LoadPlayerAnimationTables();
					}

					if (ImGui::MenuItem("Global"))
					{
						m_animations.clear();
						LoadPlayerAnimationTables();
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Toxic Caves"))
					{
						m_animations.clear();
						LoadLevelAnimations(0);
					}

					if (ImGui::MenuItem("Lava Powerhouse"))
					{
						m_animations.clear();
						LoadLevelAnimations(1);
					}

					if (ImGui::MenuItem("The Machine"))
					{
						m_animations.clear();
						LoadLevelAnimations(2);
					}

					if (ImGui::MenuItem("Showdown"))
					{
						m_animations.clear();
						LoadLevelAnimations(3);
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Scrape AnimObjects"))
					{
						m_animations.clear();
						LoadAnimationsScrapedFromAnimObjects();
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("View"))
				{
					ImGui::SetNextItemWidth(200);
					ImGui::SliderFloat("Zoom", &m_zoom, 0.0f, 8.0f, "%.1f");
					ImGui::SetNextItemWidth(200);
					ImGui::SliderFloat("Preview speed", &m_preview_speed, 0.0f, 4.0f, "%.2fx");
					ImGui::SameLine();
					if (ImGui::SmallButton("1x"))
					{
						m_preview_speed = 1.0f;
					}
					ImGui::Separator();
					ImGui::MenuItem("Show animation bounds (red)", nullptr, &m_show_animation_bounds);
					ImGui::MenuItem("Show frame bounds (yellow)", nullptr, &m_show_frame_bounds);
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Metadata"))
				{
					if (ImGui::MenuItem("Fill palettes from level objects"))
					{
						FillPalettesFromLevelObjects();
					}
					ImGui::SetItemTooltip("Gives each animation used by a level's objects that level's palette set, the object's palette line\nand its initial ticks per frame. Values you've already set are left alone.");
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

			if (m_metadata_status.empty() == false)
			{
				ImGui::TextWrapped("%s", m_metadata_status.c_str());
			}

			if (ImGui::BeginChild("animations"))
			{
				if (m_animations.empty() == false)
				{
					for (UIAnimationSequence& anim : m_animations)
					{
						char id_buffer[160];
						const Uint32 animation_offset = static_cast<rom::Ptr32>(anim.anim_sequence->rom_data.rom_offset);
						const metadata::ROMMetadata* rom_metadata = m_owning_ui.GetCurrentROMMetadata();
						const metadata::AnimationMetadata* anim_metadata = rom_metadata != nullptr ? rom_metadata->GetAnimationMetadata(animation_offset) : nullptr;
						if (anim_metadata != nullptr && anim_metadata->name.empty() == false)
						{
							snprintf(id_buffer, sizeof(id_buffer), "0x%08X  %s###0x%08X", animation_offset, anim_metadata->name.c_str(), animation_offset);
						}
						else
						{
							snprintf(id_buffer, sizeof(id_buffer), "0x%08X###0x%08X", animation_offset, animation_offset);
						}
						if (ImGui::TreeNode(id_buffer))
						{
							if (DrawMetadataEditor(anim))
							{
								m_owning_ui.SaveAnimationMetadata();
							}
							ImGui::Separator();

							char inner_buffer[256];

							for (const rom::AnimationCommand& command : anim.anim_sequence->command_sequence)
							{
								FormatCommandLabel(inner_buffer, sizeof(inner_buffer), command);

								if(ImGui::TreeNode(inner_buffer))
								{
									Uint32 current_offset = command.rom_data.rom_offset;
									Uint32 end_offset = command.rom_data.rom_offset_end;

									for (; current_offset != end_offset; current_offset += 2)
									{
										ImGui::Text("[0x%08X]:", current_offset);
										ImGui::SameLine();
										ImGui::Text("0x%04X", m_owning_ui.GetROM().ReadUint16(current_offset));
									}
									ImGui::TreePop();
								}
								if (command.ui_frame_sprite != nullptr)
								{
									command.ui_frame_sprite->DrawForImGui(2.0f);
								}
							}
							ImGui::TreePop();
						}
						else
						{
							for (int tick = 0; tick < ticks_to_run; ++tick)
							{
								anim.Tick();
							}

							const std::vector<rom::AnimationCommand>& current_anim_sequence = anim.anim_sequence->command_sequence;
							if (current_anim_sequence.empty())
							{
								continue;
							}
							const rom::AnimationCommand& current_visual_command = current_anim_sequence.at(anim.current_visual_command);

							auto& current_command = current_visual_command;
							// The box covers every frame's extent around the sprite origin, plus the animation's movement range.
							// If the animation can flip (a frame's flip bit or a flip callback), each frame's mirrored extent is included too.
							const bool can_flip_x = std::any_of(std::begin(current_anim_sequence), std::end(current_anim_sequence), [](const rom::AnimationCommand& command)
								{
									return (command.IsFrame() && command.flip_x)
										|| (command.command_type == rom::AnimationCommandType::EXECUTE_AS_FUNCTION_PTR
											&& (command.pointer == s_anim_callback_set_flip_x || command.pointer == s_anim_callback_toggle_flip_x));
								});
							float min_left = std::numeric_limits<float>::max();
							float max_right = std::numeric_limits<float>::lowest();
							float min_top = std::numeric_limits<float>::max();
							float max_bottom = std::numeric_limits<float>::lowest();
							for (const rom::AnimationCommand& command : current_anim_sequence)
							{
								const std::shared_ptr<UISpriteTexture>& frame_sprite = command.ui_frame_sprite;
								if (frame_sprite == nullptr)
								{
									continue;
								}
								const Point origin = frame_sprite->sprite->GetOriginOffsetFromMinBounds();
								const float width = frame_sprite->dimensions.x;
								const float height = frame_sprite->dimensions.y;
								min_left = std::min(min_left, -static_cast<float>(origin.x));
								max_right = std::max(max_right, width - static_cast<float>(origin.x));
								if (can_flip_x)
								{
									min_left = std::min(min_left, static_cast<float>(origin.x) - width);
									max_right = std::max(max_right, static_cast<float>(origin.x));
								}
								min_top = std::min(min_top, -static_cast<float>(origin.y));
								max_bottom = std::max(max_bottom, height - static_cast<float>(origin.y));
							}
							if (min_left > max_right)
							{
								min_left = max_right = min_top = max_bottom = 0.0f; // No frame has a sprite
							}
							const ImVec2 box_min{ min_left + anim.anim_sequence->min_dynamic_offset.x, min_top + anim.anim_sequence->min_dynamic_offset.y };
							const ImVec2 box_max{ max_right + anim.anim_sequence->max_dynamic_offset.x, max_bottom + anim.anim_sequence->max_dynamic_offset.y };

							const ImVec2 start_cursor_pos = ImGui::GetCursorPos();
							const ImVec2 start_cursor_screen_pos = ImGui::GetCursorScreenPos();
							ImGui::Dummy((box_max - box_min) * m_zoom);
							if (m_show_animation_bounds)
							{
								ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255));
							}
							if (current_command.ui_frame_sprite != nullptr && anim.frame_hidden == false)
							{
								ImGui::SameLine();
								// The sprite origin, inside the box, moved by the animation's current offset.
								const ImVec2 origin_in_box{ static_cast<float>(anim.anim_offset.x) - box_min.x, static_cast<float>(anim.anim_offset.y) - box_min.y };
								ImGui::SetCursorPos(start_cursor_pos + ImGui::GetItemRectMin() - start_cursor_screen_pos + origin_in_box * m_zoom);
								// The frame's own flip is baked into its texture; the object's flip mirrors it again around the sprite's origin,
								// as the game does (AnimObj_UpdateStateMachine XORs the two).
								const UISpriteTexture& frame_sprite = *current_command.ui_frame_sprite;
								if (frame_sprite.texture != nullptr)
								{
									const Point origin = frame_sprite.sprite->GetOriginOffsetFromMinBounds();
									const bool effective_flip_x = current_command.flip_x != anim.object_flip_x;
									const float origin_x = effective_flip_x ? frame_sprite.dimensions.x - static_cast<float>(origin.x) : static_cast<float>(origin.x);
									const ImVec2 cursor_pos = ImGui::GetCursorPos();
									ImGui::SetCursorPos(ImVec2{ cursor_pos.x - origin_x * m_zoom, cursor_pos.y - static_cast<float>(origin.y) * m_zoom });
									const float u_max = frame_sprite.dimensions.x / static_cast<float>(frame_sprite.texture->w);
									const float v_max = frame_sprite.dimensions.y / static_cast<float>(frame_sprite.texture->h);
									ImGui::Image((ImTextureID)frame_sprite.texture.get(), ImVec2{ frame_sprite.dimensions.x * m_zoom, frame_sprite.dimensions.y * m_zoom },
										ImVec2{ anim.object_flip_x ? u_max : 0.0f, 0.0f }, ImVec2{ anim.object_flip_x ? 0.0f : u_max, v_max });
								}
								if (m_show_frame_bounds)
								{
									ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 255, 0, 255));
								}
							}
						}
					}
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}
}
