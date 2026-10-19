#pragma once
#include <SimpleMath.h>

namespace Core
{
	class Vector2d
	{
	public:
		Vector2d() : x(0), y(0) {}
		Vector2d(float x, float y) : x(x), y(y) {}
		Vector2d(const Vector2d& vec) : x(vec.x), y(vec.y) {}
		Vector2d(const DirectX::SimpleMath::Vector2& v) : x(v.x), y(v.y) {}

		operator DirectX::SimpleMath::Vector2() const noexcept { return DirectX::SimpleMath::Vector2(x, y); }

		Vector2d operator *(float scalar)
		{
			Vector2d v;
			v.x = x * scalar;
			v.y = y * scalar;
			return v;
		}

		Vector2d operator +(const Vector2d& vec)
		{
			Vector2d v;
			v.x = x + vec.x;
			v.y = y + vec.y;
			return v;
		}

		Vector2d operator -(const  Vector2d& vec) const
		{
			return Vector2d(x - vec.x, y - vec.y);
		}

		bool operator ==(const Vector2d& vec) const
		{
			return x == vec.x && y == vec.y;
		}

		bool operator !=(const Vector2d& vec) const
		{
			return x != vec.x || y != vec.y;
		}
		~Vector2d() {}
	public:
		float x, y;
	};
}