#pragma once
#include <cmath>
#include <SimpleMath.h>

namespace Core
{
	class Vector3d
	{
	public:
		Vector3d() : x(0.0f), y(0.0f), z(0.0f) {}
		Vector3d(float x, float y, float z) :x(x), y(y), z(z) {}
		Vector3d(const  Vector3d& vector) :x(vector.x), y(vector.y), z(vector.z) {}
		Vector3d(const DirectX::SimpleMath::Vector3& v) : x(v.x), y(v.y), z(v.z) {}

		operator DirectX::SimpleMath::Vector3() const noexcept { return DirectX::SimpleMath::Vector3(x, y, z); }

		static Vector3d lerp(const  Vector3d& start, const  Vector3d& end, float delta)
		{
			Vector3d v;
			v.x = start.x * (1.0f - delta) + end.x * (delta);
			v.y = start.y * (1.0f - delta) + end.y * (delta);
			v.z = start.z * (1.0f - delta) + end.z * (delta);
			return v;
		}

		Vector3d operator *(float num) const
		{
			return Vector3d(x * num, y * num, z * num);
		}

		Vector3d operator +(const  Vector3d& vec) const
		{
			return Vector3d(x + vec.x, y + vec.y, z + vec.z);
		}

		Vector3d operator -(const  Vector3d& vec) const
		{
			return Vector3d(x - vec.x, y - vec.y, z - vec.z);
		}

		Vector3d operator /(float num) const
		{
			return Vector3d(x / num, y / num, z / num);
		}

		Vector3d operator -() const
		{
			return Vector3d(-x, -y, -z);
		}

		Vector3d& operator +=(const Vector3d& vec)
		{
			x += vec.x; y += vec.y; z += vec.z;
			return *this;
		}

		Vector3d& operator -=(const Vector3d& vec)
		{
			x -= vec.x; y -= vec.y; z -= vec.z;
			return *this;
		}

		Vector3d& operator *=(float num)
		{
			x *= num; y *= num; z *= num;
			return *this;
		}

		Vector3d& operator /=(float num)
		{
			x /= num; y /= num; z /= num;
			return *this;
		}

		bool operator !=(const  Vector3d& vec) const
		{
			return vec.x != x || vec.y != y || vec.z != z;
		}

		bool operator ==(const Vector3d& vec) const
		{
			return vec.x == x && vec.y == y && vec.z == z;
		}



		float length() const
		{
			return (float)std::sqrt((x * x) + (y * y) + (z * z));
		}

		float sqrLength() const
		{
			return (x * x) + (y * y) + (z * z);
		}

		static float distance(const Vector3d& a, const Vector3d& b)
		{
			return (a - b).length();
		}

		static float dot(const Vector3d& a, const Vector3d& b)
		{
			return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
		}

		static Vector3d normalize(const  Vector3d& vec)
		{
			Vector3d res;
			float len = (float)std::sqrt((float)(vec.x * vec.x) + (float)(vec.y * vec.y) + (float)(vec.z * vec.z));
			if (!len)
				return Vector3d();

			res.x = vec.x / len;
			res.y = vec.y / len;
			res.z = vec.z / len;

			return res;
		}

		static Vector3d cross(const  Vector3d& v1, const  Vector3d& v2)
		{
			Vector3d res;
			res.x = (v1.y * v2.z) - (v1.z * v2.y);
			res.y = (v1.z * v2.x) - (v1.x * v2.z);
			res.z = (v1.x * v2.y) - (v1.y * v2.x);
			return res;
		}
	public:
		float x, y, z;
	};
}