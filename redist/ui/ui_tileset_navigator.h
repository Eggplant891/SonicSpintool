#pragma once

#include "ui_editor_window.h"

#include <memory>
#include <vector>

namespace spintool
{
	class EditorUI;
	namespace rom
	{
		struct TileSet;
		class SpinballROM;
	}
}

namespace spintool
{
	class EditorTilesetNavigator : public EditorWindowBase
	{
	public:
		using EditorWindowBase::EditorWindowBase;
		
		void Update() override;

		std::vector<std::shared_ptr<const rom::TileSet>> m_tilesets;
	};
}