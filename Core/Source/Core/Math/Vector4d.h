#pragma once
#include "Vector3d.h"
namespace Core
{
	class Vector4d
	{
	public:
		Vector4d() :x(0), y(0), z(0), w(0)
		{
		}
		Vector4d(float x, float y, float z, float w) :x(x), y(y), z(z), w(w)
		{
		}
		Vector4d(const Vector4d& vector) :x(vector.x), y(vector.y), z(vector.z), w(vector.w)
		{
		}

		Vector4d(const Vector3d& vector) :x(vector.x), y(vector.y), z(vector.z), w(1)
		{
		}

		void cross(Vector4d& v1, Vector4d& v2, Vector4d& v3)
		{
			this->x = v1.y * (v2.z * v3.w - v3.z * v2.w) - v1.z * (v2.y * v3.w - v3.y * v2.w) + v1.w * (v2.y * v3.z - v2.z * v3.y);
			this->y = -(v1.x * (v2.z * v3.w - v3.z * v2.w) - v1.z * (v2.x * v3.w - v3.x * v2.w) + v1.w * (v2.x * v3.z - v3.x * v2.z));
			this->z = v1.x * (v2.y * v3.w - v3.y * v2.w) - v1.y * (v2.x * v3.w - v3.x * v2.w) + v1.w * (v2.x * v3.y - v3.x * v2.y);
			this->w = -(v1.x * (v2.y * v3.z - v3.y * v2.z) - v1.y * (v2.x * v3.z - v3.x * v2.z) + v1.z * (v2.x * v3.y - v3.x * v2.y));
		}

		~Vector4d()
		{
		}

	public:
		float x ,y ,z ,w;
	};
}