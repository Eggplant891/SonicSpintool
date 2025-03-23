#pragma once
#include <algorithm>

namespace spintool
{
	struct Point
	{
		int x = 0;
		int y = 0;

		bool operator==(const Point& rhs) const
		{
			return x == rhs.x && y == rhs.y;
		}

		void operator *=(const float scalar)
		{
			x = static_cast<int>(x * scalar);
			y *= static_cast<int>(y * scalar);
		}
	};

	struct BoundingBox
	{
		Point min{ 0,0 };
		Point max{ 0,0 };

		int Width() const;
		int Height() const;

		Point MinOrdered() const
		{
			return Point{ std::min(min.x, max.x), std::min(min.y, max.y) };
		}

		Point MaxOrdered() const
		{
			return Point{ std::max(min.x, max.x), std::max(min.y, max.y) };
		}

		bool operator==(const BoundingBox& rhs) const
		{
			return min == rhs.min && max == rhs.max;
		}

		void operator *=(const float scalar)
		{
			min *= scalar;
			max *= scalar;
		}
	};
}