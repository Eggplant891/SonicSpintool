#pragma once

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
	};

	struct BoundingBox
	{
		Point min{ 0,0 };
		Point max{ 0,0 };

		int Width() const;
		int Height() const;

		bool operator==(const BoundingBox& rhs) const
		{
			return min == rhs.min && max == rhs.max;
		}
	};
}