#pragma once

namespace spintool
{
	class EditorUI;
}

namespace spintool
{
	class EditorWindowBase
	{
	public:
		EditorWindowBase(EditorUI& owning_ui);

		virtual void Update() = 0;
		bool m_visible = false;

	protected:
		EditorUI& m_owning_ui;
	};
}