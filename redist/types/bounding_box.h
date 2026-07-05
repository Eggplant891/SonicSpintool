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

		bool operator==(const Point& rhs) const;
		[[nodiscard]] operator ImVec2();
	};

	[[nodiscard]] inline Point operator+(const Point& lhs, const Point& rhs) { return Point{ lhs.x + rhs.x, lhs.y + rhs.y }; }
	[[nodiscard]] inline Point operator-(const Point& lhs, const Point& rhs) { return Point{ lhs.x - rhs.x, lhs.y - rhs.y }; }
	[[nodiscard]] inline Point operator+(const Point& lhs, const ImVec2& rhs) { return Point{ static_cast<int>(static_cast<float>(lhs.x) + rhs.x), static_cast<int>(static_cast<float>(lhs.y) + rhs.y) }; }
	[[nodiscard]] inline Point operator-(const Point& lhs, const ImVec2& rhs) { return Point{ static_cast<int>(static_cast<float>(lhs.x) - rhs.x), static_cast<int>(static_cast<float>(lhs.y) - rhs.y) }; }
	[[nodiscard]] inline Point operator+(const ImVec2& lhs, const Point& rhs) { return Point{ static_cast<int>(static_cast<float>(lhs.x) + rhs.x), static_cast<int>(static_cast<float>(lhs.y) + rhs.y) }; }
	[[nodiscard]] inline Point operator-(const ImVec2& lhs, const Point& rhs) { return Point{ static_cast<int>(static_cast<float>(lhs.x) - rhs.x), static_cast<int>(static_cast<float>(lhs.y) - rhs.y) }; }
	[[nodiscard]] inline Point operator*(const Point& lhs, const float rhs) { return Point{ static_cast<int>(lhs.x * rhs), static_cast<int>(lhs.y * rhs) }; }
	[[nodiscard]] inline Point operator/(const Point& lhs, const float rhs) { return Point{ static_cast<int>(lhs.x / rhs), static_cast<int>(lhs.y / rhs) }; }

	struct BoundingBox
	{
		Point min{ 0,0 };
		Point max{ 0,0 };

		[[nodiscard]] int Width() const;
		[[nodiscard]] int Height() const;

		[[nodiscard]] Point MinOrdered() const;
		[[nodiscard]] Point MaxOrdered() const;

		operator ImRect();

		bool operator==(const BoundingBox& rhs) const;
		void operator *=(float scalar);
	};

	[[nodiscard]] static BoundingBox operator+(const BoundingBox& lhs, const Point& rhs) { return  BoundingBox{ lhs.min.x + rhs.x, lhs.min.y + rhs.y, lhs.max.x + rhs.x, lhs.max.y + rhs.y }; }
	[[nodiscard]] static BoundingBox operator-(const BoundingBox& lhs, const Point& rhs) { return  BoundingBox{ lhs.min.x - rhs.x, lhs.min.y - rhs.y, lhs.max.x - rhs.x, lhs.max.y - rhs.y }; }
	[[nodiscard]] static BoundingBox operator+(const BoundingBox& lhs, const ImVec2& rhs) { return BoundingBox{ static_cast<int>(static_cast<float>(lhs.min.x) + rhs.x), static_cast<int>(static_cast<float>(lhs.min.y) + rhs.y), static_cast<int>(static_cast<float>(lhs.max.x) + rhs.x), static_cast<int>(static_cast<float>(lhs.max.y) + rhs.y) }; }
	[[nodiscard]] static BoundingBox operator-(const BoundingBox& lhs, const ImVec2& rhs) { return BoundingBox{ static_cast<int>(static_cast<float>(lhs.min.x) - rhs.x), static_cast<int>(static_cast<float>(lhs.min.y) - rhs.y), static_cast<int>(static_cast<float>(lhs.max.x) - rhs.x), static_cast<int>(static_cast<float>(lhs.max.y) - rhs.y) }; }
	[[nodiscard]] static BoundingBox operator+(const ImVec2& rhs, const BoundingBox& lhs) { return BoundingBox{ static_cast<int>(static_cast<float>(lhs.min.x) + rhs.x), static_cast<int>(static_cast<float>(lhs.min.y) + rhs.y), static_cast<int>(static_cast<float>(lhs.max.x) + rhs.x), static_cast<int>(static_cast<float>(lhs.max.y) + rhs.y) }; }
	[[nodiscard]] static BoundingBox operator-(const ImVec2& rhs, const BoundingBox& lhs) { return BoundingBox{ static_cast<int>(static_cast<float>(lhs.min.x) - rhs.x), static_cast<int>(static_cast<float>(lhs.min.y) - rhs.y), static_cast<int>(static_cast<float>(lhs.max.x) - rhs.x), static_cast<int>(static_cast<float>(lhs.max.y) - rhs.y) }; }
	[[nodiscard]] static BoundingBox operator*(const BoundingBox& lhs, const float rhs) { return   BoundingBox{ lhs.min * rhs, lhs.max * rhs }; }
	[[nodiscard]] static BoundingBox operator/(const BoundingBox& lhs, const float rhs) { return   BoundingBox{ lhs.min / rhs, lhs.max / rhs }; }
}
