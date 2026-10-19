#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Math/Vector3d.h"
#include "Engine/Core/Math/Vector4d.h"
#include "Engine/Core/Random.h"
#include <cmath>
#include <cstring>

namespace
{
	// Legacy Mat4x4 reference implementation for differential testing
	class LegacyMat4x4
	{
	public:
		LegacyMat4x4() { setIdentity(); }
		LegacyMat4x4(float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33)
		{
			m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
			m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
			m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
			m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
		}

		void setIdentity()
		{
			std::memset(m, 0, sizeof(float) * 16);
			m[0][0] = 1.0f;
			m[1][1] = 1.0f;
			m[2][2] = 1.0f;
			m[3][3] = 1.0f;
		}

		void setTranslation(const Core::Vector3d& v)
		{
			m[3][0] = v.x;
			m[3][1] = v.y;
			m[3][2] = v.z;
		}

		void setScale(const Core::Vector3d& v)
		{
			m[0][0] = v.x;
			m[1][1] = v.y;
			m[2][2] = v.z;
		}

		void setRotationX(float angle)
		{
			setIdentity();
			m[1][1] = std::cos(angle);
			m[1][2] = std::sin(angle);
			m[2][1] = -std::sin(angle);
			m[2][2] = std::cos(angle);
		}

		void setRotationY(float angle)
		{
			setIdentity();
			m[0][0] = std::cos(angle);
			m[0][2] = -std::sin(angle);
			m[2][0] = std::sin(angle);
			m[2][2] = std::cos(angle);
		}

		void setRotationZ(float angle)
		{
			setIdentity();
			m[0][0] = std::cos(angle);
			m[0][1] = std::sin(angle);
			m[1][0] = -std::sin(angle);
			m[1][1] = std::cos(angle);
		}

		void setPerspectiveFovLH(float fov, float aspect, float znear, float zfar)
		{
			setIdentity();
			float yscale = 1.0f / std::tan(fov / 2.0f);
			float xscale = yscale / aspect;
			m[0][0] = xscale;
			m[1][1] = yscale;
			m[2][2] = zfar / (zfar - znear);
			m[2][3] = 1.0f;
			m[3][2] = (-znear * zfar) / (zfar - znear);
			m[3][3] = 0.0f; // Correct perspective projection divide (legacy setIdentity left 1.0f)
		}

		void setOrthoLH(float width, float height, float near_plane, float far_plane)
		{
			setIdentity();
			m[0][0] = 2.0f / width;
			m[1][1] = 2.0f / height;
			m[2][2] = 1.0f / (far_plane - near_plane);
			m[3][2] = -(near_plane / (far_plane - near_plane));
		}

		LegacyMat4x4 operator*(const LegacyMat4x4& matrix) const
		{
			LegacyMat4x4 temp;
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					temp.m[i][j] = m[i][0] * matrix.m[0][j] +
						m[i][1] * matrix.m[1][j] +
						m[i][2] * matrix.m[2][j] +
						m[i][3] * matrix.m[3][j];
				}
			}
			return temp;
		}

		void setMatrix(const LegacyMat4x4& matrix)
		{
			std::memcpy(m, matrix.m, sizeof(float) * 16);
		}

		Core::Vector3d getRightwardDirection() const
		{
			return Core::Vector3d(m[0][0], m[0][1], m[0][2]);
		}
		Core::Vector3d getUpDirection() const
		{
			return Core::Vector3d(m[1][0], m[1][1], m[1][2]);
		}
		Core::Vector3d getForwardDirection() const
		{
			return Core::Vector3d(m[2][0], m[2][1], m[2][2]);
		}
		Core::Vector3d getTranslation() const
		{
			return Core::Vector3d(m[3][0], m[3][1], m[3][2]);
		}

		float getDeterminant() const
		{
			Core::Vector4d minor, v1, v2, v3;
			float det;

			v1 = Core::Vector4d(this->m[0][0], this->m[1][0], this->m[2][0], this->m[3][0]);
			v2 = Core::Vector4d(this->m[0][1], this->m[1][1], this->m[2][1], this->m[3][1]);
			v3 = Core::Vector4d(this->m[0][2], this->m[1][2], this->m[2][2], this->m[3][2]);

			minor.cross(v1, v2, v3);
			det = -(this->m[0][3] * minor.x + this->m[1][3] * minor.y + this->m[2][3] * minor.z +
				this->m[3][3] * minor.w);
			return det;
		}

		void inverse()
		{
			int a, i, j;
			LegacyMat4x4 out;
			Core::Vector4d v, vec[3];
			float det = this->getDeterminant();
			if (std::abs(det) < 1e-8f) return;

			for (i = 0; i < 4; i++)
			{
				for (j = 0; j < 4; j++)
				{
					if (j != i)
					{
						a = j;
						if (j > i) a = a - 1;
						vec[a].x = (this->m[j][0]);
						vec[a].y = (this->m[j][1]);
						vec[a].z = (this->m[j][2]);
						vec[a].w = (this->m[j][3]);
					}
				}
				v.cross(vec[0], vec[1], vec[2]);

				float sign = ((i % 2 == 0) ? 1.0f : -1.0f);
				out.m[0][i] = sign * v.x / det;
				out.m[1][i] = sign * v.y / det;
				out.m[2][i] = sign * v.z / det;
				out.m[3][i] = sign * v.w / det;
			}

			this->setMatrix(out);
		}

		union
		{
			float m[4][4];
			float mat[4][4];
		};
	};

	inline void CheckMatrixEqual(const LegacyMat4x4& legacy, const DirectX::SimpleMath::Matrix& sm, float eps = 1e-4f)
	{
		for (int r = 0; r < 4; ++r)
		{
			for (int c = 0; c < 4; ++c)
			{
				CAPTURE(r);
				CAPTURE(c);
				CHECK(legacy.m[r][c] == doctest::Approx(sm.m[r][c]).epsilon(eps));
			}
		}
	}
}

TEST_CASE("Math::Differential - LegacyMat4x4 vs SimpleMath across 1,000+ Inputs")
{
	Core::Random rng(133742);

	SUBCASE("1,000 Randomized Translation Matrices")
	{
		for (int i = 0; i < 1000; ++i)
		{
			float x = rng.nextFloatRange(-1000.0f, 1000.0f);
			float y = rng.nextFloatRange(-1000.0f, 1000.0f);
			float z = rng.nextFloatRange(-1000.0f, 1000.0f);

			LegacyMat4x4 leg;
			leg.setTranslation(Core::Vector3d(x, y, z));

			DirectX::SimpleMath::Matrix sm = DirectX::SimpleMath::Matrix::CreateTranslation(x, y, z);
			CheckMatrixEqual(leg, sm);
		}
	}

	SUBCASE("1,000 Randomized Scale Matrices")
	{
		for (int i = 0; i < 1000; ++i)
		{
			float sx = rng.nextFloatRange(0.01f, 100.0f);
			float sy = rng.nextFloatRange(0.01f, 100.0f);
			float sz = rng.nextFloatRange(0.01f, 100.0f);

			LegacyMat4x4 leg;
			leg.setScale(Core::Vector3d(sx, sy, sz));

			DirectX::SimpleMath::Matrix sm = DirectX::SimpleMath::Matrix::CreateScale(sx, sy, sz);
			CheckMatrixEqual(leg, sm);
		}
	}

	SUBCASE("1,000 Randomized Rotations (X, Y, Z)")
	{
		for (int i = 0; i < 1000; ++i)
		{
			float angle = rng.nextFloatRange(-Core::Math::TWO_PI, Core::Math::TWO_PI);

			LegacyMat4x4 legX, legY, legZ;
			legX.setRotationX(angle);
			legY.setRotationY(angle);
			legZ.setRotationZ(angle);

			DirectX::SimpleMath::Matrix smX = DirectX::SimpleMath::Matrix::CreateRotationX(angle);
			DirectX::SimpleMath::Matrix smY = DirectX::SimpleMath::Matrix::CreateRotationY(angle);
			DirectX::SimpleMath::Matrix smZ = DirectX::SimpleMath::Matrix::CreateRotationZ(angle);

			CheckMatrixEqual(legX, smX);
			CheckMatrixEqual(legY, smY);
			CheckMatrixEqual(legZ, smZ);
		}
	}

	SUBCASE("1,000 Randomized Matrix Multiplications")
	{
		for (int i = 0; i < 1000; ++i)
		{
			float x1 = rng.nextFloatRange(-10.0f, 10.0f);
			float y1 = rng.nextFloatRange(-10.0f, 10.0f);
			float z1 = rng.nextFloatRange(-10.0f, 10.0f);
			float ax = rng.nextFloatRange(-Core::Math::PI, Core::Math::PI);
			float ay = rng.nextFloatRange(-Core::Math::PI, Core::Math::PI);

			LegacyMat4x4 legT, legRx, legRy;
			legT.setTranslation(Core::Vector3d(x1, y1, z1));
			legRx.setRotationX(ax);
			legRy.setRotationY(ay);
			LegacyMat4x4 legProd = legT * legRx * legRy;

			DirectX::SimpleMath::Matrix smT = DirectX::SimpleMath::Matrix::CreateTranslation(x1, y1, z1);
			DirectX::SimpleMath::Matrix smRx = DirectX::SimpleMath::Matrix::CreateRotationX(ax);
			DirectX::SimpleMath::Matrix smRy = DirectX::SimpleMath::Matrix::CreateRotationY(ay);
			DirectX::SimpleMath::Matrix smProd = smT * smRx * smRy;

			CheckMatrixEqual(legProd, smProd);
		}
	}

	SUBCASE("1,000 Randomized Perspective Field of View LH")
	{
		for (int i = 0; i < 1000; ++i)
		{
			float fov = rng.nextFloatRange(0.1f, 2.5f);
			float aspect = rng.nextFloatRange(0.5f, 3.0f);
			float nearZ = rng.nextFloatRange(0.01f, 10.0f);
			float farZ = nearZ + rng.nextFloatRange(10.0f, 1000.0f);

			LegacyMat4x4 leg;
			leg.setPerspectiveFovLH(fov, aspect, nearZ, farZ);

			DirectX::SimpleMath::Matrix sm = Core::Math::perspectiveFovLH(fov, aspect, nearZ, farZ);
			CheckMatrixEqual(leg, sm);
		}
	}

	SUBCASE("1,000 Randomized Orthographic LH")
	{
		for (int i = 0; i < 1000; ++i)
		{
			float w = rng.nextFloatRange(10.0f, 1920.0f);
			float h = rng.nextFloatRange(10.0f, 1080.0f);
			float nearZ = rng.nextFloatRange(0.0f, 10.0f);
			float farZ = nearZ + rng.nextFloatRange(10.0f, 1000.0f);

			LegacyMat4x4 leg;
			leg.setOrthoLH(w, h, nearZ, farZ);

			DirectX::SimpleMath::Matrix sm = Core::Math::orthographicLH(w, h, nearZ, farZ);
			CheckMatrixEqual(leg, sm);
		}
	}

	SUBCASE("1,000 Randomized Determinant & Inversion Checks")
	{
		for (int i = 0; i < 1000; ++i)
		{
			float tx = rng.nextFloatRange(-50.0f, 50.0f);
			float ty = rng.nextFloatRange(-50.0f, 50.0f);
			float tz = rng.nextFloatRange(-50.0f, 50.0f);
			float rotAngle = rng.nextFloatRange(-Core::Math::PI, Core::Math::PI);
			float s = rng.nextFloatRange(0.5f, 5.0f);

			LegacyMat4x4 legT, legR, legS;
			legT.setTranslation(Core::Vector3d(tx, ty, tz));
			legR.setRotationY(rotAngle);
			legS.setScale(Core::Vector3d(s, s, s));
			LegacyMat4x4 legMat = legS * legR * legT;

			DirectX::SimpleMath::Matrix smMat =
				DirectX::SimpleMath::Matrix::CreateScale(s, s, s) *
				DirectX::SimpleMath::Matrix::CreateRotationY(rotAngle) *
				DirectX::SimpleMath::Matrix::CreateTranslation(tx, ty, tz);

			float legDet = legMat.getDeterminant();
			float smDet = smMat.Determinant();
			CHECK(legDet == doctest::Approx(smDet).epsilon(1e-3f));

			legMat.inverse();
			DirectX::SimpleMath::Matrix smInv = smMat.Invert();
			CheckMatrixEqual(legMat, smInv, 1e-3f);
		}
	}

	SUBCASE("Direction Extraction Vectors")
	{
		for (int i = 0; i < 100; ++i)
		{
			float angle = rng.nextFloatRange(-Core::Math::PI, Core::Math::PI);
			LegacyMat4x4 leg;
			leg.setRotationY(angle);

			DirectX::SimpleMath::Matrix sm = DirectX::SimpleMath::Matrix::CreateRotationY(angle);

			Core::Vector3d legFwd = leg.getForwardDirection();
			Core::Vector3 smFwd = Core::Math::getForward(sm);
			CHECK(legFwd.x == doctest::Approx(smFwd.x).epsilon(1e-4f));
			CHECK(legFwd.y == doctest::Approx(smFwd.y).epsilon(1e-4f));
			CHECK(legFwd.z == doctest::Approx(smFwd.z).epsilon(1e-4f));

			Core::Vector3d legUp = leg.getUpDirection();
			Core::Vector3 smUp = Core::Math::getUp(sm);
			CHECK(legUp.x == doctest::Approx(smUp.x).epsilon(1e-4f));
			CHECK(legUp.y == doctest::Approx(smUp.y).epsilon(1e-4f));
			CHECK(legUp.z == doctest::Approx(smUp.z).epsilon(1e-4f));

			Core::Vector3d legRight = leg.getRightwardDirection();
			Core::Vector3 smRight = Core::Math::getRight(sm);
			CHECK(legRight.x == doctest::Approx(smRight.x).epsilon(1e-4f));
			CHECK(legRight.y == doctest::Approx(smRight.y).epsilon(1e-4f));
			CHECK(legRight.z == doctest::Approx(smRight.z).epsilon(1e-4f));
		}
	}
}

TEST_CASE("Math::ReversedZ - Left-Handed Reversed-Z Depth Mapping")
{
	float fovY = Core::Math::toRadians(60.0f);
	float aspect = 16.0f / 9.0f;
	float nearZ = 0.1f;
	float farZ = 1000.0f;

	auto proj = Core::Math::perspectiveFovLH_ReversedZ(fovY, aspect, nearZ, farZ);

	SUBCASE("Near plane maps to 1.0f")
	{
		DirectX::SimpleMath::Vector4 nearPoint(0.0f, 0.0f, nearZ, 1.0f);
		DirectX::SimpleMath::Vector4 clip = DirectX::SimpleMath::Vector4::Transform(nearPoint, proj);
		float ndcZ = clip.z / clip.w;
		CHECK(ndcZ == doctest::Approx(1.0f).epsilon(1e-5f));
	}

	SUBCASE("Far plane maps to 0.0f")
	{
		DirectX::SimpleMath::Vector4 farPoint(0.0f, 0.0f, farZ, 1.0f);
		DirectX::SimpleMath::Vector4 clip = DirectX::SimpleMath::Vector4::Transform(farPoint, proj);
		float ndcZ = clip.z / clip.w;
		CHECK(std::abs(ndcZ) < 1e-5f);
	}

	SUBCASE("Mid plane depth decreases monotonically")
	{
		DirectX::SimpleMath::Vector4 p1(0.0f, 0.0f, 1.0f, 1.0f);
		DirectX::SimpleMath::Vector4 p2(0.0f, 0.0f, 10.0f, 1.0f);
		DirectX::SimpleMath::Vector4 p3(0.0f, 0.0f, 100.0f, 1.0f);

		auto c1 = DirectX::SimpleMath::Vector4::Transform(p1, proj);
		auto c2 = DirectX::SimpleMath::Vector4::Transform(p2, proj);
		auto c3 = DirectX::SimpleMath::Vector4::Transform(p3, proj);

		float z1 = c1.z / c1.w;
		float z2 = c2.z / c2.w;
		float z3 = c3.z / c3.w;

		CHECK(z1 > z2);
		CHECK(z2 > z3);
		CHECK(z1 < 1.0f);
		CHECK(z3 > 0.0f);
	}

	SUBCASE("Orthographic Reversed-Z maps nearZ to 1.0f and farZ to 0.0f")
	{
		float width = 800.0f;
		float height = 600.0f;
		float oNear = 0.5f;
		float oFar = 50.0f;

		auto ortho = Core::Math::orthographicLH_ReversedZ(width, height, oNear, oFar);

		DirectX::SimpleMath::Vector4 pNear(0.0f, 0.0f, oNear, 1.0f);
		DirectX::SimpleMath::Vector4 pFar(0.0f, 0.0f, oFar, 1.0f);

		auto cNear = DirectX::SimpleMath::Vector4::Transform(pNear, ortho);
		auto cFar = DirectX::SimpleMath::Vector4::Transform(pFar, ortho);

		CHECK(cNear.z == doctest::Approx(1.0f).epsilon(1e-5f));
		CHECK(std::abs(cFar.z) < 1e-5f);
	}
}

TEST_CASE("Math::Operations - LookAtLH, TRS Decomposition, and Euler")
{
	SUBCASE("LookAtLH camera orientation")
	{
		DirectX::SimpleMath::Vector3 eye(0.0f, 0.0f, -10.0f);
		DirectX::SimpleMath::Vector3 target(0.0f, 0.0f, 0.0f);
		DirectX::SimpleMath::Vector3 up(0.0f, 1.0f, 0.0f);

		auto view = Core::Math::lookAtLH(eye, target, up);

		// Target should be at (0, 0, +10) in camera view space
		DirectX::SimpleMath::Vector4 targetWorld(0.0f, 0.0f, 0.0f, 1.0f);
		auto targetView = DirectX::SimpleMath::Vector4::Transform(targetWorld, view);
		CHECK(targetView.x == doctest::Approx(0.0f).epsilon(1e-4f));
		CHECK(targetView.y == doctest::Approx(0.0f).epsilon(1e-4f));
		CHECK(targetView.z == doctest::Approx(10.0f).epsilon(1e-4f));
	}

	SUBCASE("TRS decompose and reconstruct")
	{
		DirectX::SimpleMath::Vector3 origT(15.0f, -25.0f, 50.0f);
		DirectX::SimpleMath::Quaternion origR = DirectX::SimpleMath::Quaternion::CreateFromAxisAngle(DirectX::SimpleMath::Vector3(0, 1, 0), Core::Math::toRadians(45.0f));
		DirectX::SimpleMath::Vector3 origS(2.0f, 3.0f, 4.0f);

		auto trs = Core::Math::createTransformTRS(origT, origR, origS);

		DirectX::SimpleMath::Vector3 outT, outS;
		DirectX::SimpleMath::Quaternion outR;
		bool success = Core::Math::decomposeTRS(trs, outT, outR, outS);

		CHECK(success);
		CHECK(outT.x == doctest::Approx(origT.x).epsilon(1e-4f));
		CHECK(outT.y == doctest::Approx(origT.y).epsilon(1e-4f));
		CHECK(outT.z == doctest::Approx(origT.z).epsilon(1e-4f));

		CHECK(outS.x == doctest::Approx(origS.x).epsilon(1e-4f));
		CHECK(outS.y == doctest::Approx(origS.y).epsilon(1e-4f));
		CHECK(outS.z == doctest::Approx(origS.z).epsilon(1e-4f));

		// Reconstruct and compare
		auto recon = Core::Math::createTransformTRS(outT, outR, outS);
		for (int r = 0; r < 4; ++r)
		{
			for (int c = 0; c < 4; ++c)
			{
				CHECK(trs.m[r][c] == doctest::Approx(recon.m[r][c]).epsilon(1e-4f));
			}
		}
	}

	SUBCASE("Euler to Quaternion order Scale -> RotX -> RotY -> RotZ -> Translation")
	{
		DirectX::SimpleMath::Vector3 t(1.0f, 2.0f, 3.0f);
		DirectX::SimpleMath::Vector3 rotEuler(Core::Math::toRadians(30.0f), Core::Math::toRadians(45.0f), Core::Math::toRadians(60.0f));
		DirectX::SimpleMath::Vector3 s(1.0f, 1.0f, 1.0f);

		auto matEuler = Core::Math::createTransformEuler(t, rotEuler, s);
		auto q = Core::Math::eulerToQuaternion(rotEuler.x, rotEuler.y, rotEuler.z);
		auto matQuat = Core::Math::createTransformTRS(t, q, s);

		for (int r = 0; r < 4; ++r)
		{
			for (int c = 0; c < 4; ++c)
			{
				CHECK(matEuler.m[r][c] == doctest::Approx(matQuat.m[r][c]).epsilon(1e-4f));
			}
		}
	}

	SUBCASE("BoundingFrustum culling")
	{
		auto proj = Core::Math::perspectiveFovLH(Core::Math::toRadians(60.0f), 1.0f, 0.1f, 100.0f);
		DirectX::BoundingFrustum frustum;
		DirectX::BoundingFrustum::CreateFromMatrix(frustum, proj);

		// Inside frustum
		DirectX::BoundingBox boxInside(DirectX::SimpleMath::Vector3(0, 0, 10), DirectX::SimpleMath::Vector3(1, 1, 1));
		CHECK(frustum.Contains(boxInside) != DirectX::DISJOINT);

		// Behind camera
		DirectX::BoundingBox boxBehind(DirectX::SimpleMath::Vector3(0, 0, -10), DirectX::SimpleMath::Vector3(1, 1, 1));
		CHECK(frustum.Contains(boxBehind) == DirectX::DISJOINT);
	}
}
