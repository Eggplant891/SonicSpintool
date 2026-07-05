#include "types/bounding_box.h"
#include <valarray>

namespace spintool
{
	bool Point::operator==(const Point& rhs) const
	{
		return x == rhs.x && y == rhs.y;
	}

	[[nodiscard]] Point::operator ImVec2()
	{
		return ImVec2{ static_cast<float>(x), static_cast<float>(y) };
	}

	int BoundingBox::Width() const
	{
		return std::abs(max.x - min.x);
	}

	int BoundingBox::Height() const
	{
		return std::abs(max.y - min.y);
	}

	[[nodiscard]] Point BoundingBox::MinOrdered() const
	{
		return Point{ std::min(min.x, max.x), std::min(min.y, max.y) };
	}

	[[nodiscard]] Point BoundingBox::MaxOrdered() const
	{
		return Point{ std::max(min.x, max.x), std::max(min.y, max.y) };
	}

	BoundingBox::operator ImRect()
	{
		return ImRect{ static_cast<ImVec2>(min), static_cast<ImVec2>(max) };
	}

	bool BoundingBox::operator==(const BoundingBox& rhs) const
	{
		return min == rhs.min && max == rhs.max;
	}

	void BoundingBox::operator *=(const float scalar)
	{
		min = min * scalar;
		max = max * scalar;
	}
}