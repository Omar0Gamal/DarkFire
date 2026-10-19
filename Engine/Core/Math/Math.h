#pragma once

#include "Engine/Core/Defines.h"
#include <SimpleMath.h>
#include <DirectXCollision.h>
#include <cmath>

namespace Core
{
	using Matrix = DirectX::SimpleMath::Matrix;
	using Mat4x4 = DirectX::SimpleMath::Matrix;
	using Mat4 = DirectX::SimpleMath::Matrix;
	using Vector2 = DirectX::SimpleMath::Vector2;
	using Vector3 = DirectX::SimpleMath::Vector3;
	using Vector4 = DirectX::SimpleMath::Vector4;
	using Quaternion = DirectX::SimpleMath::Quaternion;
	using Plane = DirectX::SimpleMath::Plane;
	using Ray = DirectX::SimpleMath::Ray;
	using Color = DirectX::SimpleMath::Color;

	using BoundingBox = DirectX::BoundingBox;
	using BoundingFrustum = DirectX::BoundingFrustum;
	using BoundingSphere = DirectX::BoundingSphere;
	using BoundingOrientedBox = DirectX::BoundingOrientedBox;

	namespace Math
	{
		using Vector2 = Core::Vector2;
		using Vector3 = Core::Vector3;
		using Vector4 = Core::Vector4;
		using Matrix = Core::Matrix;
		using Mat4x4 = Core::Mat4x4;
		using Quaternion = Core::Quaternion;
		using Color = Core::Color;

		constexpr float PI = 3.14159265358979323846f;
		constexpr float TWO_PI = 6.28318530717958647692f;
		constexpr float HALF_PI = 1.57079632679489661923f;
		constexpr float DEG2RAD = PI / 180.0f;
		constexpr float RAD2DEG = 180.0f / PI;

		inline float toRadians(float degrees) noexcept { return degrees * DEG2RAD; }
		inline float toDegrees(float radians) noexcept { return radians * RAD2DEG; }

		// Left-Handed Direction extraction (+X Right, +Y Up, +Z Forward)
		inline Vector3 getRight(const Matrix& m) noexcept { return Vector3(m.m[0][0], m.m[0][1], m.m[0][2]); }
		inline Vector3 getUp(const Matrix& m) noexcept { return Vector3(m.m[1][0], m.m[1][1], m.m[1][2]); }
		inline Vector3 getForward(const Matrix& m) noexcept { return Vector3(m.m[2][0], m.m[2][1], m.m[2][2]); }
		inline Vector3 getTranslation(const Matrix& m) noexcept { return Vector3(m.m[3][0], m.m[3][1], m.m[3][2]); }

		// Left-Handed View Matrix (row vectors: v_view = v_world * V)
		inline Matrix lookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up) noexcept
		{
			DirectX::XMVECTOR eyeV = eye;
			DirectX::XMVECTOR targetV = target;
			DirectX::XMVECTOR upV = up;
			return DirectX::XMMatrixLookAtLH(eyeV, targetV, upV);
		}

		// Standard Left-Handed Perspective Projection (nearZ -> 0.0f, farZ -> 1.0f)
		inline Matrix perspectiveFovLH(float fovY, float aspect, float nearZ, float farZ) noexcept
		{
			return DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
		}

		// Standard Left-Handed Orthographic Projection (nearZ -> 0.0f, farZ -> 1.0f)
		inline Matrix orthographicLH(float width, float height, float nearZ, float farZ) noexcept
		{
			return DirectX::XMMatrixOrthographicLH(width, height, nearZ, farZ);
		}

		// Left-Handed Reversed-Z Perspective Projection (nearZ -> 1.0f, farZ -> 0.0f)
		inline Matrix perspectiveFovLH_ReversedZ(float fovY, float aspect, float nearZ, float farZ) noexcept
		{
			DF_ASSERT(nearZ > 0.0f && farZ > nearZ, "nearZ must be > 0 and farZ > nearZ");
			float yscale = 1.0f / std::tan(fovY * 0.5f);
			float xscale = yscale / aspect;

			Matrix m = {};
			m.m[0][0] = xscale;
			m.m[1][1] = yscale;
			m.m[2][2] = nearZ / (nearZ - farZ); // equivalent to -nearZ / (farZ - nearZ)
			m.m[2][3] = 1.0f;
			m.m[3][2] = (nearZ * farZ) / (farZ - nearZ);
			m.m[3][3] = 0.0f;
			return m;
		}

		// Left-Handed Reversed-Z Orthographic Projection (nearZ -> 1.0f, farZ -> 0.0f)
		inline Matrix orthographicLH_ReversedZ(float width, float height, float nearZ, float farZ) noexcept
		{
			DF_ASSERT(farZ > nearZ, "farZ must be greater than nearZ");
			Matrix m = DirectX::XMMatrixIdentity();
			m.m[0][0] = 2.0f / width;
			m.m[1][1] = 2.0f / height;
			m.m[2][2] = -1.0f / (farZ - nearZ);
			m.m[3][2] = farZ / (farZ - nearZ);
			return m;
		}

		// Fast Affine Transform Inverse (inverts TRS matrix)
		inline Matrix affineInverse(const Matrix& m) noexcept
		{
			DirectX::XMVECTOR det;
			return DirectX::XMMatrixInverse(&det, m);
		}

		// Decompose TRS Matrix into Translation, Rotation Quaternion, and Scale
		inline bool decomposeTRS(const Matrix& m, Vector3& outTranslation, Quaternion& outRotation, Vector3& outScale) noexcept
		{
			return const_cast<Matrix&>(m).Decompose(outScale, outRotation, outTranslation);
		}

		// Construct TRS Matrix from components
		inline Matrix createTransformTRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale) noexcept
		{
			return Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(translation);
		}

		// Euler-to-Quaternion matching Scale -> RotX -> RotY -> RotZ -> Translation
		inline Quaternion eulerToQuaternion(float pitchX, float yawY, float rollZ) noexcept
		{
			Matrix rot = Matrix::CreateRotationX(pitchX) * Matrix::CreateRotationY(yawY) * Matrix::CreateRotationZ(rollZ);
			return Quaternion::CreateFromRotationMatrix(rot);
		}

		// Construct Transform Matrix from Euler Angles
		inline Matrix createTransformEuler(const Vector3& translation, const Vector3& rotationEuler, const Vector3& scale) noexcept
		{
			Matrix s = Matrix::CreateScale(scale);
			Matrix rx = Matrix::CreateRotationX(rotationEuler.x);
			Matrix ry = Matrix::CreateRotationY(rotationEuler.y);
			Matrix rz = Matrix::CreateRotationZ(rotationEuler.z);
			Matrix t = Matrix::CreateTranslation(translation);
			return s * rx * ry * rz * t;
		}
	}
}
