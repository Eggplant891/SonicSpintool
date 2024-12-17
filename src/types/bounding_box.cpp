#include "types/bounding_box.h"

namespace spintool
{
	int BoundingBox::Width() const
	{
		return max.x - min.x;
	}

	int BoundingBox::Height() const
	{
		return max.y - min.y;
	}
}