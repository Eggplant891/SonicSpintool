#pragma once
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
	class EditorTilesetNavigator
	{
	public:
		EditorTilesetNavigator(EditorUI& owning_ui);

		void Update();

		bool visible = false;
		std::vector<std::shared_ptr<const rom::TileSet>> m_tilesets;

	private:
		EditorUI& m_owning_ui;
		rom::SpinballROM& m_rom;

	};
}