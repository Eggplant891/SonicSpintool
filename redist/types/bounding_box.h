#pragma once
#include <algorithm>
#include "imgui.h"
#include "imgui_internal.h"

namespace spintool
{
	struct Point
	{
		int x = 0;
		int y = 0;

		void operator+=(const Point& rhs) { x += rhs.x; y += rhs.y; }
		void operator-=(const Point& rhs) { x -= rhs.x; y -= rhs.y; }
		void operator*=(const int rhs) { x *= rhs; y *= rhs; }
		void operator/=(const int rhs) { x /= rhs; y /= rhs; }

		bool operator==(const Point& rhs) const
		{
			return x == rhs.x && y == rhs.y;
		}

		operator ImVec2()
		{
			return ImVec2{ static_cast<float>(x), static_cast<float>(y) };
		}
	};

	static Point operator+(const Point& lhs, const Point& rhs) { return Point{ lhs.x + rhs.x, lhs.y + rhs.y }; }
	static Point operator-(const Point& lhs, const Point& rhs) { return Point{ lhs.x - rhs.x, lhs.y - rhs.y }; }
	static Point operator+(const Point& lhs, const ImVec2& rhs) { return Point{ static_cast<int>(static_cast<float>(lhs.x) + rhs.x), static_cast<int>(static_cast<float>(lhs.y) + rhs.y) }; }
	static Point operator-(const Point& lhs, const ImVec2& rhs) { return Point{ static_cast<int>(static_cast<float>(lhs.x) - rhs.x), static_cast<int>(static_cast<float>(lhs.y) - rhs.y) }; }
	static Point operator+(const ImVec2& rhs, const Point& lhs) { return Point{ static_cast<int>(static_cast<float>(lhs.x) + rhs.x), static_cast<int>(static_cast<float>(lhs.y) + rhs.y) }; }
	static Point operator-(const ImVec2& rhs, const Point& lhs) { return Point{ static_cast<int>(static_cast<float>(lhs.x) - rhs.x), static_cast<int>(static_cast<float>(lhs.y) - rhs.y) }; }
	static Point operator*(const Point& lhs, const float rhs) { return Point{ static_cast<int>(lhs.x * rhs), static_cast<int>(lhs.y * rhs) }; }
	static Point operator/(const Point& lhs, const float rhs) { return Point{ static_cast<int>(lhs.x / rhs), static_cast<int>(lhs.y / rhs) }; }

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

		operator ImRect()
		{
			return ImRect{ min, max };
		}

		bool operator==(const BoundingBox& rhs) const
		{
			return min == rhs.min && max == rhs.max;
		}

		void operator *=(const float scalar)
		{
			min = min * scalar;
			max = max * scalar;
		}
	};
}