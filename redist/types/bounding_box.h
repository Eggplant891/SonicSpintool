#pragma once

namespace spintool
{
	struct Point
	{
		int x = 0;
		int y = 0;
	};

	struct BoundingBox
	{
		Point min{ 0,0 };
		Point max{ 0,0 };

		int Width() const;
		int Height() const;
	};
}