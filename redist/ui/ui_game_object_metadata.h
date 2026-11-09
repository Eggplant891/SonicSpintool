#pragma once

#include "ui/ui_editor_window.h"

#include "SDL3/SDL_stdinc.h"

#include <optional>

namespace spintool::metadata
{
	struct GameObjectMetadata;
}

namespace spintool
{
	// Returns true once an edit has been committed and should be saved.
	bool DrawGameObjectMetadataEditor(metadata::GameObjectMetadata& obj_metadata);
	void DrawGameObjectMetadataSummary(const metadata::GameObjectMetadata* obj_metadata, Uint16 flags);

	class EditorGameObjectMetadataWindow : public EditorWindowBase
	{
	public:
		using EditorWindowBase::EditorWindowBase;

		void Update() override;

	private:
		int m_level_index = 0;
		bool m_show_all_types = false;
		std::optional<Uint8> m_selected_type;
	};
}
