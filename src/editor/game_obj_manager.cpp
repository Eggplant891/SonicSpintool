#include "editor/game_obj_manager.h"

namespace spintool
{

	void GameObject::MoveObjectTo(int x, int y)
	{
		m_bbox.max.x = x - m_origin.x + m_bbox.Width();
		m_bbox.min.x = x - m_origin.x;

		m_bbox.max.y = y - m_origin.x + m_bbox.Height();
		m_bbox.min.y = y - m_origin.x;
	}

	void GameObject::MoveObjectRelative(int x, int y)
	{

	}
}