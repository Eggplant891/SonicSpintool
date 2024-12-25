#include "ui/ui_editor_window.h"

namespace spintool
{
	EditorWindowBase::EditorWindowBase(EditorUI& owning_ui)
		: m_owning_ui(owning_ui)
	{

	}

	bool EditorWindowBase::IsOpen() const
	{
		return m_visible;
	}

}