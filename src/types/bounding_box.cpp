#include "types/bounding_box.h"
#include <valarray>

namespace spintool
{
	int BoundingBox::Width() const
	{
		return std::abs(max.x - min.x);
	}

	int BoundingBox::Height() const
	{
		return std::abs(max.y - min.y);
	}
}