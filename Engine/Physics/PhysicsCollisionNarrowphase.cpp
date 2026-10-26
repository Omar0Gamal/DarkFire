#include "Engine/Physics/BVH.h"
#include "Engine/Physics/PhysicsCollisionNarrowphase.h"
#include <algorithm>
#include <cmath>
#include <limits>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Physics
{
	namespace
	{
		bool TestSphereSphere(
			const Core::ColliderComponent& colA, const Core::TransformComponent& transA,
			const Core::ColliderComponent& colB, const Core::TransformComponent& transB,
			Vector3& outNormal, float& outPenetration
		)
		{
			Vector3 delta = transB.position - transA.position;
			float distSq = delta.LengthSquared();
			float radiusSum = colA.radius + colB.radius;
			if (distSq < radiusSum * radiusSum)
			{
				float dist = std::sqrt(distSq);
				outNormal = (dist > 0.0001f) ? (delta / dist) : Vector3(0.0f, 1.0f, 0.0f);
				outPenetration = radiusSum - dist;
				return true;
			}
			return false;
		}

		bool TestBoxSphere(
			const Core::ColliderComponent& colBox, const Core::TransformComponent& transBox,
			const Core::ColliderComponent& colSphere, const Core::TransformComponent& transSphere,
			Vector3& outNormal, float& outPenetration
		)
		{
			Vector3 halfBox = colBox.size * 0.5f;
			Quaternion invRotBox;
			transBox.rotation.Inverse(invRotBox);
			Vector3 localSpherePos = Vector3::Transform(transSphere.position - transBox.position, invRotBox);

			Vector3 localClamped = Vector3(
				std::clamp(localSpherePos.x, -halfBox.x, halfBox.x),
				std::clamp(localSpherePos.y, -halfBox.y, halfBox.y),
				std::clamp(localSpherePos.z, -halfBox.z, halfBox.z)
			);
			Vector3 localDelta = localSpherePos - localClamped;
			float distSq = localDelta.LengthSquared();

			if (distSq > 0.000001f)
			{
				if (distSq < colSphere.radius * colSphere.radius)
				{
					float dist = std::sqrt(distSq);
					Vector3 localNorm = localDelta / dist;
					outNormal = Vector3::Transform(localNorm, transBox.rotation);
					outPenetration = colSphere.radius - dist;
					return true;
				}
				return false;
			}
			else
			{
				// Sphere center is inside Box: resolve along closest exit face in local space
				float distPosX = halfBox.x - localSpherePos.x;
				float distNegX = localSpherePos.x - (-halfBox.x);
				float distPosY = halfBox.y - localSpherePos.y;
				float distNegY = localSpherePos.y - (-halfBox.y);
				float distPosZ = halfBox.z - localSpherePos.z;
				float distNegZ = localSpherePos.z - (-halfBox.z);

				float minDist = distPosY;
				Vector3 localNorm = Vector3(0.0f, 1.0f, 0.0f);

				if (distPosX < minDist) { minDist = distPosX; localNorm = Vector3(1.0f, 0.0f, 0.0f); }
				if (distNegX < minDist) { minDist = distNegX; localNorm = Vector3(-1.0f, 0.0f, 0.0f); }
				if (distPosY < minDist) { minDist = distPosY; localNorm = Vector3(0.0f, 1.0f, 0.0f); }
				if (distNegY < minDist) { minDist = distNegY; localNorm = Vector3(0.0f, -1.0f, 0.0f); }
				if (distPosZ < minDist) { minDist = distPosZ; localNorm = Vector3(0.0f, 0.0f, 1.0f); }
				if (distNegZ < minDist) { minDist = distNegZ; localNorm = Vector3(0.0f, 0.0f, -1.0f); }

				outNormal = Vector3::Transform(localNorm, transBox.rotation);
				outPenetration = colSphere.radius + minDist;
				return true;
			}
		}

		bool TestCapsuleSphere(
			const Core::ColliderComponent& colCap, const Core::TransformComponent& transCap,
			const Core::ColliderComponent& colSphere, const Core::TransformComponent& transSphere,
			Vector3& outNormal, float& outPenetration
		)
		{
			float halfSeg = std::max(0.0f, colCap.height * 0.5f - colCap.radius);
			Vector3 up = transCap.getUp();
			Vector3 p0 = transCap.position - up * halfSeg;
			Vector3 p1 = transCap.position + up * halfSeg;

			Vector3 v = p1 - p0;
			Vector3 w = transSphere.position - p0;
			float vLenSq = v.LengthSquared();
			float t = (vLenSq > 1e-6f) ? std::clamp(w.Dot(v) / vLenSq, 0.0f, 1.0f) : 0.0f;
			Vector3 closestCapPt = p0 + v * t;

			Vector3 delta = transSphere.position - closestCapPt;
			float distSq = delta.LengthSquared();
			float radiusSum = colCap.radius + colSphere.radius;

			if (distSq < radiusSum * radiusSum)
			{
				float dist = std::sqrt(distSq);
				if (dist > 0.0001f)
				{
					outNormal = delta / dist;
				}
				else
				{
					Vector3 perp = Vector3::Transform(Vector3(1.0f, 0.0f, 0.0f), transCap.rotation);
					if (std::abs(perp.Dot(up)) > 0.9f)
					{
						perp = Vector3::Transform(Vector3(0.0f, 0.0f, 1.0f), transCap.rotation);
					}
					outNormal = perp;
				}
				outPenetration = radiusSum - dist;
				return true;
			}
			return false;
		}

		bool TestCapsuleCapsule(
			const Core::ColliderComponent& colA, const Core::TransformComponent& transA,
			const Core::ColliderComponent& colB, const Core::TransformComponent& transB,
			Vector3& outNormal, float& outPenetration
		)
		{
			float halfSegA = std::max(0.0f, colA.height * 0.5f - colA.radius);
			Vector3 upA = transA.getUp();
			Vector3 p0 = transA.position - upA * halfSegA;
			Vector3 p1 = transA.position + upA * halfSegA;
			Vector3 u = p1 - p0;

			float halfSegB = std::max(0.0f, colB.height * 0.5f - colB.radius);
			Vector3 upB = transB.getUp();
			Vector3 q0 = transB.position - upB * halfSegB;
			Vector3 q1 = transB.position + upB * halfSegB;
			Vector3 v = q1 - q0;

			Vector3 w0 = p0 - q0;
			float a = u.Dot(u);
			float b = u.Dot(v);
			float c = v.Dot(v);
			float d = u.Dot(w0);
			float e = v.Dot(w0);

			float s = 0.0f;
			float t = 0.0f;

			if (a < 1e-6f && c < 1e-6f)
			{
				s = 0.0f;
				t = 0.0f;
			}
			else if (a < 1e-6f)
			{
				s = 0.0f;
				t = std::clamp(e / c, 0.0f, 1.0f);
			}
			else if (c < 1e-6f)
			{
				t = 0.0f;
				s = std::clamp(-d / a, 0.0f, 1.0f);
			}
			else
			{
				float denom = a * c - b * b;
				if (denom < 1e-6f)
				{
					s = 0.0f;
					t = std::clamp(e / c, 0.0f, 1.0f);
				}
				else
				{
					s = std::clamp((b * e - c * d) / denom, 0.0f, 1.0f);
					t = (b * s + e) / c;
					if (t < 0.0f)
					{
						t = 0.0f;
						s = std::clamp(-d / a, 0.0f, 1.0f);
					}
					else if (t > 1.0f)
					{
						t = 1.0f;
						s = std::clamp((b - d) / a, 0.0f, 1.0f);
					}
				}
			}

			Vector3 cA = p0 + u * s;
			Vector3 cB = q0 + v * t;

			Vector3 delta = cB - cA;
			float distSq = delta.LengthSquared();
			float radiusSum = colA.radius + colB.radius;

			if (distSq < radiusSum * radiusSum)
			{
				float dist = std::sqrt(distSq);
				if (dist > 0.0001f)
				{
					outNormal = delta / dist;
				}
				else
				{
					Vector3 perp = u.Cross(v);
					if (perp.LengthSquared() > 1e-6f)
					{
						perp.Normalize();
						if ((transB.position - transA.position).Dot(perp) < 0.0f) perp = -perp;
						outNormal = perp;
					}
					else
					{
						Vector3 fallback = transB.position - transA.position;
						if (fallback.LengthSquared() > 1e-6f)
						{
							fallback.Normalize();
							outNormal = fallback;
						}
						else
						{
							outNormal = Vector3(0.0f, 1.0f, 0.0f);
						}
					}
				}
				outPenetration = radiusSum - dist;
				return true;
			}
			return false;
		}

		bool TestBoxCapsule(
			const Core::ColliderComponent& colBox, const Core::TransformComponent& transBox,
			const Core::ColliderComponent& colCap, const Core::TransformComponent& transCap,
			Vector3& outNormal, float& outPenetration
		)
		{
			Vector3 halfBox = colBox.size * 0.5f;
			float halfSeg = std::max(0.0f, colCap.height * 0.5f - colCap.radius);
			Vector3 upCap = transCap.getUp();
			Vector3 p0World = transCap.position - upCap * halfSeg;
			Vector3 p1World = transCap.position + upCap * halfSeg;

			Quaternion invRotBox;
			transBox.rotation.Inverse(invRotBox);
			Vector3 p0Local = Vector3::Transform(p0World - transBox.position, invRotBox);
			Vector3 p1Local = Vector3::Transform(p1World - transBox.position, invRotBox);
			Vector3 uLocal = p1Local - p0Local;

			// 1. Slab test of line segment against Box AABB [-halfBox, halfBox]
			float tMinSlab = 0.0f;
			float tMaxSlab = 1.0f;
			bool segmentIntersectsBox = true;

			for (int i = 0; i < 3; ++i)
			{
				float p0Comp = (i == 0) ? p0Local.x : (i == 1 ? p0Local.y : p0Local.z);
				float uComp  = (i == 0) ? uLocal.x  : (i == 1 ? uLocal.y  : uLocal.z);
				float hComp  = (i == 0) ? halfBox.x : (i == 1 ? halfBox.y : halfBox.z);

				if (std::abs(uComp) < 1e-6f)
				{
					if (p0Comp < -hComp || p0Comp > hComp)
					{
						segmentIntersectsBox = false;
						break;
					}
				}
				else
				{
					float t1 = (-hComp - p0Comp) / uComp;
					float t2 = (hComp - p0Comp) / uComp;
					if (t1 > t2) std::swap(t1, t2);
					tMinSlab = std::max(tMinSlab, t1);
					tMaxSlab = std::min(tMaxSlab, t2);
					if (tMinSlab > tMaxSlab)
					{
						segmentIntersectsBox = false;
						break;
					}
				}
			}

			if (segmentIntersectsBox)
			{
				// Segment penetrates or touches Box AABB
				float tInside = (tMinSlab + tMaxSlab) * 0.5f;
				Vector3 localPt = p0Local + uLocal * tInside;

				// Resolve along closest exit face in local space
				float distPosX = halfBox.x - localPt.x;
				float distNegX = localPt.x - (-halfBox.x);
				float distPosY = halfBox.y - localPt.y;
				float distNegY = localPt.y - (-halfBox.y);
				float distPosZ = halfBox.z - localPt.z;
				float distNegZ = localPt.z - (-halfBox.z);

				float minDist = distPosY;
				Vector3 localNorm = Vector3(0.0f, 1.0f, 0.0f);

				if (distPosX < minDist) { minDist = distPosX; localNorm = Vector3(1.0f, 0.0f, 0.0f); }
				if (distNegX < minDist) { minDist = distNegX; localNorm = Vector3(-1.0f, 0.0f, 0.0f); }
				if (distPosY < minDist) { minDist = distPosY; localNorm = Vector3(0.0f, 1.0f, 0.0f); }
				if (distNegY < minDist) { minDist = distNegY; localNorm = Vector3(0.0f, -1.0f, 0.0f); }
				if (distPosZ < minDist) { minDist = distPosZ; localNorm = Vector3(0.0f, 0.0f, 1.0f); }
				if (distNegZ < minDist) { minDist = distNegZ; localNorm = Vector3(0.0f, 0.0f, -1.0f); }

				outNormal = Vector3::Transform(localNorm, transBox.rotation);
				outPenetration = colCap.radius + minDist;
				return true;
			}

			// Segment is completely outside box: find closest point on segment to Box AABB
			auto distSqToAABB = [&](float tVal) -> float {
				Vector3 pt = p0Local + uLocal * tVal;
				Vector3 clamped(
					std::clamp(pt.x, -halfBox.x, halfBox.x),
					std::clamp(pt.y, -halfBox.y, halfBox.y),
					std::clamp(pt.z, -halfBox.z, halfBox.z)
				);
				return (pt - clamped).LengthSquared();
			};

			float aVal = 0.0f;
			float bVal = 1.0f;
			constexpr float phiRatio = 0.6180339887f;
			float cVal = bVal - phiRatio * (bVal - aVal);
			float dVal = aVal + phiRatio * (bVal - aVal);
			float fc = distSqToAABB(cVal);
			float fd = distSqToAABB(dVal);

			for (int iter = 0; iter < 24; ++iter)
			{
				if (fc < fd)
				{
					bVal = dVal;
					dVal = cVal;
					fd = fc;
					cVal = bVal - phiRatio * (bVal - aVal);
					fc = distSqToAABB(cVal);
				}
				else
				{
					aVal = cVal;
					cVal = dVal;
					fc = fd;
					dVal = aVal + phiRatio * (bVal - aVal);
					fd = distSqToAABB(dVal);
				}
			}
			float bestT = (aVal + bVal) * 0.5f;
			float minVal = distSqToAABB(bestT);

			auto checkCandT = [&](float candT) {
				if (candT >= 0.0f && candT <= 1.0f)
				{
					float val = distSqToAABB(candT);
					if (val < minVal)
					{
						minVal = val;
						bestT = candT;
					}
				}
			};
			checkCandT(0.0f);
			checkCandT(1.0f);

			Vector3 closestLocalPt = p0Local + uLocal * bestT;
			Vector3 localClamped = Vector3(
				std::clamp(closestLocalPt.x, -halfBox.x, halfBox.x),
				std::clamp(closestLocalPt.y, -halfBox.y, halfBox.y),
				std::clamp(closestLocalPt.z, -halfBox.z, halfBox.z)
			);
			Vector3 localDelta = closestLocalPt - localClamped;
			float distSq = localDelta.LengthSquared();

			if (distSq < colCap.radius * colCap.radius)
			{
				float dist = std::sqrt(distSq);
				Vector3 localNorm = (dist > 0.0001f) ? (localDelta / dist) : Vector3(0.0f, 1.0f, 0.0f);
				outNormal = Vector3::Transform(localNorm, transBox.rotation);
				outPenetration = colCap.radius - dist;
				return true;
			}

			return false;
		}
	}

	OBB MakeOBB(const Core::TransformComponent& trans, const Core::ColliderComponent& col)
	{
		OBB obb{};
		obb.center = trans.position;
		obb.extents = col.size * 0.5f;
		obb.axes[0] = trans.getRight();
		obb.axes[1] = trans.getUp();
		obb.axes[2] = trans.getForward();
		return obb;
	}

	bool TestOBBOBB(const OBB& a, const OBB& b, Vector3& outNormal, float& outPenetration)
	{
		Vector3 D = b.center - a.center;

		float minOverlap = (std::numeric_limits<float>::max)();
		Vector3 bestAxis(0.0f, 1.0f, 0.0f);

		// Helper to test a single candidate axis L
		auto testAxis = [&](Vector3 L, float bias = 1.0f) -> bool
		{
			float lenSq = L.LengthSquared();
			if (lenSq < 0.0001f) return true; // Degenerate cross product (parallel edges)

			L /= std::sqrt(lenSq);

			float rA = a.extents.x * std::abs(a.axes[0].Dot(L))
			         + a.extents.y * std::abs(a.axes[1].Dot(L))
			         + a.extents.z * std::abs(a.axes[2].Dot(L));

			float rB = b.extents.x * std::abs(b.axes[0].Dot(L))
			         + b.extents.y * std::abs(b.axes[1].Dot(L))
			         + b.extents.z * std::abs(b.axes[2].Dot(L));

			float s = std::abs(D.Dot(L));
			float overlap = (rA + rB) - s;

			if (overlap <= 0.0f)
			{
				return false; // Separating axis found!
			}

			if (overlap * bias < minOverlap)
			{
				minOverlap = overlap * bias;
				bestAxis = L;
			}
			return true;
		};

		// 1. Box A face normals (3) - priority
		if (!testAxis(a.axes[0], 1.0f)) return false;
		if (!testAxis(a.axes[1], 1.0f)) return false;
		if (!testAxis(a.axes[2], 1.0f)) return false;

		// 2. Box B face normals (3) - priority
		if (!testAxis(b.axes[0], 1.0f)) return false;
		if (!testAxis(b.axes[1], 1.0f)) return false;
		if (!testAxis(b.axes[2], 1.0f)) return false;

		// 3. Edge cross products (3 x 3 = 9) - slight bias to prevent jittering on flat faces
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				Vector3 axis = a.axes[i].Cross(b.axes[j]);
				if (!testAxis(axis, 1.05f)) return false;
			}
		}

		// Ensure normal points from A to B
		if (D.Dot(bestAxis) < 0.0f)
		{
			bestAxis = -bestAxis;
		}
		else if (std::abs(D.Dot(bestAxis)) < 0.00001f)
		{
			Vector3 diff = b.center - a.center;
			if (diff.LengthSquared() > 0.0001f)
			{
				diff.Normalize();
				if (diff.Dot(bestAxis) < 0.0f)
				{
					bestAxis = -bestAxis;
				}
			}
		}

		outNormal = bestAxis;
		outPenetration = minOverlap;
		return true;
	}

	void ComputeColliderWorldAABB(
		const Core::ColliderComponent& col,
		const Core::TransformComponent& trans,
		Vector3& outMin,
		Vector3& outMax
	)
	{
		if (col.shape == Core::ColliderShape::Sphere)
		{
			Vector3 rVec(col.radius, col.radius, col.radius);
			outMin = trans.position - rVec;
			outMax = trans.position + rVec;
		}
		else if (col.shape == Core::ColliderShape::Box)
		{
			OBB obb = MakeOBB(trans, col);
			Vector3 hx(
				obb.extents.x * std::abs(obb.axes[0].x) + obb.extents.y * std::abs(obb.axes[1].x) + obb.extents.z * std::abs(obb.axes[2].x),
				obb.extents.x * std::abs(obb.axes[0].y) + obb.extents.y * std::abs(obb.axes[1].y) + obb.extents.z * std::abs(obb.axes[2].y),
				obb.extents.x * std::abs(obb.axes[0].z) + obb.extents.y * std::abs(obb.axes[1].z) + obb.extents.z * std::abs(obb.axes[2].z)
			);
			outMin = obb.center - hx;
			outMax = obb.center + hx;
		}
		else if (col.shape == Core::ColliderShape::Capsule)
		{
			float halfSeg = std::max(0.0f, col.height * 0.5f - col.radius);
			Vector3 up = trans.getUp();
			Vector3 p0 = trans.position - up * halfSeg;
			Vector3 p1 = trans.position + up * halfSeg;

			Vector3 minSeg(
				std::min(p0.x, p1.x),
				std::min(p0.y, p1.y),
				std::min(p0.z, p1.z)
			);
			Vector3 maxSeg(
				std::max(p0.x, p1.x),
				std::max(p0.y, p1.y),
				std::max(p0.z, p1.z)
			);

			Vector3 rVec(col.radius, col.radius, col.radius);
			outMin = minSeg - rVec;
			outMax = maxSeg + rVec;
		}
		else
		{
			outMin = trans.position - Vector3(0.5f, 0.5f, 0.5f);
			outMax = trans.position + Vector3(0.5f, 0.5f, 0.5f);
		}
	}

		Vector3 ClosestPointOnTriangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c)
	{
		Vector3 ab = b - a;
		Vector3 ac = c - a;
		Vector3 ap = p - a;

		float d1 = ab.Dot(ap);
		float d2 = ac.Dot(ap);
		if (d1 <= 0.0f && d2 <= 0.0f) return a;

		Vector3 bp = p - b;
		float d3 = ab.Dot(bp);
		float d4 = ac.Dot(bp);
		if (d3 >= 0.0f && d4 <= d3) return b;

		float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
		{
			float v = d1 / (d1 - d3);
			return a + ab * v;
		}

		Vector3 cp = p - c;
		float d5 = ab.Dot(cp);
		float d6 = ac.Dot(cp);
		if (d6 >= 0.0f && d5 <= d6) return c;

		float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
		{
			float w = d2 / (d2 - d6);
			return a + ac * w;
		}

		float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
		{
			float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			return b + (c - b) * w;
		}

		float denom = 1.0f / (va + vb + vc);
		float v = vb * denom;
		float w = vc * denom;
		return a + ab * v + ac * w;
	}

	bool TestSphereTriangle(
		const Vector3& sphereCenter, float sphereRadius,
		const Vector3& a, const Vector3& b, const Vector3& c,
		Vector3& outNormal, float& outPenetration)
	{
		Vector3 closest = ClosestPointOnTriangle(sphereCenter, a, b, c);
		Vector3 diff = sphereCenter - closest;
		float distSq = diff.LengthSquared();
		if (distSq > sphereRadius * sphereRadius)
		{
			return false;
		}

		float dist = std::sqrt(distSq);
		if (dist > 1e-6f)
		{
			outNormal = diff / dist;
			outPenetration = sphereRadius - dist;
		}
		else
		{
			Vector3 e1 = b - a;
			Vector3 e2 = c - a;
			Vector3 n = e1.Cross(e2);
			float nlen = n.Length();
			outNormal = (nlen > 1e-6f) ? (n / nlen) : Vector3(0.0f, 1.0f, 0.0f);
			outPenetration = sphereRadius;
		}
		return true;
	}

	bool TestOBBTriangle(
		const OBB& obb,
		const Vector3& a, const Vector3& b, const Vector3& c,
		Vector3& outNormal, float& outPenetration)
	{
		Vector3 triNormal = (b - a).Cross(c - a);
		float triNormLen = triNormal.Length();
		if (triNormLen < 1e-6f) return false;
		triNormal /= triNormLen;

		Vector3 edges[3] = { b - a, c - b, a - c };

		Vector3 axes[13];
		int axisCount = 0;

		axes[axisCount++] = triNormal;
		axes[axisCount++] = obb.axes[0];
		axes[axisCount++] = obb.axes[1];
		axes[axisCount++] = obb.axes[2];

		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				Vector3 crossAxis = obb.axes[i].Cross(edges[j]);
				if (crossAxis.LengthSquared() > 1e-6f)
				{
					crossAxis.Normalize();
					axes[axisCount++] = crossAxis;
				}
			}
		}

		float minOverlap = 1e30f;
		Vector3 bestAxis = Vector3(0.0f, 1.0f, 0.0f);

		for (int i = 0; i < axisCount; ++i)
		{
			const Vector3& axis = axes[i];

			float pA = a.Dot(axis);
			float pB = b.Dot(axis);
			float pC = c.Dot(axis);
			float tMin = std::min(pA, std::min(pB, pC));
			float tMax = std::max(pA, std::max(pB, pC));

			float obbCenterProj = obb.center.Dot(axis);
			float obbRadius = obb.extents.x * std::abs(obb.axes[0].Dot(axis)) +
			                  obb.extents.y * std::abs(obb.axes[1].Dot(axis)) +
			                  obb.extents.z * std::abs(obb.axes[2].Dot(axis));

			float oMin = obbCenterProj - obbRadius;
			float oMax = obbCenterProj + obbRadius;

			if (tMin > oMax || oMin > tMax)
			{
				return false;
			}

			float overlap = std::min(tMax, oMax) - std::max(tMin, oMin);
			if (overlap < minOverlap)
			{
				minOverlap = overlap;
				bestAxis = axis;
				if (obbCenterProj < 0.5f * (tMin + tMax))
				{
					bestAxis = -bestAxis;
				}
			}
		}

		outPenetration = minOverlap;
		outNormal = bestAxis;
		return true;
	}

	bool TestSphereTriangleMesh(
		const Core::ColliderComponent& sphereCol, const Core::TransformComponent& sphereTrans,
		const Core::ColliderComponent& meshCol, const Core::TransformComponent& meshTrans,
		Vector3& outNormal, float& outPenetration
	)
	{
		if (!meshCol.collisionMesh || !meshCol.collisionMesh->bvh) return false;

		Matrix worldMesh = meshTrans.getLocalMatrix();
		Matrix invWorldMesh = worldMesh.Invert();

		Vector3 localSphereCenter = Vector3::Transform(sphereTrans.position, invWorldMesh);
		float avgScale = (meshTrans.scale.x + meshTrans.scale.y + meshTrans.scale.z) / 3.0f;
		float localSphereRadius = sphereCol.radius / (avgScale > 1e-4f ? avgScale : 1.0f);

		std::vector<uint32_t> candidateTris;
		meshCol.collisionMesh->bvh->querySphere(localSphereCenter, localSphereRadius, candidateTris);

		bool hit = false;
		float maxPen = -1e30f;
		Vector3 bestNormal;

		for (uint32_t triIdx : candidateTris)
		{
			const auto& tri = meshCol.collisionMesh->bvh->getTriangles()[triIdx];
			Vector3 n;
			float pen = 0.0f;
			if (TestSphereTriangle(localSphereCenter, localSphereRadius, tri.v0, tri.v1, tri.v2, n, pen))
			{
				if (pen > maxPen)
				{
					maxPen = pen;
					bestNormal = n;
					hit = true;
				}
			}
		}

		if (hit)
		{
			outPenetration = maxPen * avgScale;
			outNormal = Vector3::TransformNormal(bestNormal, worldMesh);
			outNormal.Normalize();
		}
		return hit;
	}

	bool TestOBBTriangleMesh(
		const Core::ColliderComponent& boxCol, const Core::TransformComponent& boxTrans,
		const Core::ColliderComponent& meshCol, const Core::TransformComponent& meshTrans,
		Vector3& outNormal, float& outPenetration
	)
	{
		if (!meshCol.collisionMesh || !meshCol.collisionMesh->bvh) return false;

		OBB obb = MakeOBB(boxTrans, boxCol);
		Matrix worldMesh = meshTrans.getLocalMatrix();
		Matrix invWorldMesh = worldMesh.Invert();

		OBB localOBB;
		localOBB.center = Vector3::Transform(obb.center, invWorldMesh);
		for (int i = 0; i < 3; ++i)
		{
			localOBB.axes[i] = Vector3::TransformNormal(obb.axes[i], invWorldMesh);
			float len = localOBB.axes[i].Length();
			if (len > 1e-6f) localOBB.axes[i] /= len;
		}
		localOBB.extents = Vector3(
			obb.extents.x / std::max(meshTrans.scale.x, 1e-4f),
			obb.extents.y / std::max(meshTrans.scale.y, 1e-4f),
			obb.extents.z / std::max(meshTrans.scale.z, 1e-4f)
		);

		Vector3 localOBBAABBMin(
			localOBB.center.x - (std::abs(localOBB.axes[0].x) * localOBB.extents.x + std::abs(localOBB.axes[1].x) * localOBB.extents.y + std::abs(localOBB.axes[2].x) * localOBB.extents.z),
			localOBB.center.y - (std::abs(localOBB.axes[0].y) * localOBB.extents.x + std::abs(localOBB.axes[1].y) * localOBB.extents.y + std::abs(localOBB.axes[2].y) * localOBB.extents.z),
			localOBB.center.z - (std::abs(localOBB.axes[0].z) * localOBB.extents.x + std::abs(localOBB.axes[1].z) * localOBB.extents.y + std::abs(localOBB.axes[2].z) * localOBB.extents.z)
		);
		Vector3 localOBBAABBMax(
			localOBB.center.x + (std::abs(localOBB.axes[0].x) * localOBB.extents.x + std::abs(localOBB.axes[1].x) * localOBB.extents.y + std::abs(localOBB.axes[2].x) * localOBB.extents.z),
			localOBB.center.y + (std::abs(localOBB.axes[0].y) * localOBB.extents.x + std::abs(localOBB.axes[1].y) * localOBB.extents.y + std::abs(localOBB.axes[2].y) * localOBB.extents.z),
			localOBB.center.z + (std::abs(localOBB.axes[0].z) * localOBB.extents.x + std::abs(localOBB.axes[1].z) * localOBB.extents.y + std::abs(localOBB.axes[2].z) * localOBB.extents.z)
		);

		std::vector<uint32_t> candidateTris;
		meshCol.collisionMesh->bvh->queryAABB(localOBBAABBMin, localOBBAABBMax, candidateTris);

		bool hit = false;
		float maxPen = -1e30f;
		Vector3 bestNormal;

		for (uint32_t triIdx : candidateTris)
		{
			const auto& tri = meshCol.collisionMesh->bvh->getTriangles()[triIdx];
			Vector3 n;
			float pen = 0.0f;
			if (TestOBBTriangle(localOBB, tri.v0, tri.v1, tri.v2, n, pen))
			{
				if (pen > maxPen)
				{
					maxPen = pen;
					bestNormal = n;
					hit = true;
				}
			}
		}

		if (hit)
		{
			float avgScale = (meshTrans.scale.x + meshTrans.scale.y + meshTrans.scale.z) / 3.0f;
			outPenetration = maxPen * avgScale;
			outNormal = Vector3::TransformNormal(bestNormal, worldMesh);
			outNormal.Normalize();
		}
		return hit;
	}

bool TestColliderPair(
		const Core::ColliderComponent& colA, const Core::TransformComponent& transA,
		const Core::ColliderComponent& colB, const Core::TransformComponent& transB,
		Vector3& outNormal, float& outPenetration
	)
	{
		// 1. Sphere vs Sphere
		if (colA.shape == Core::ColliderShape::Sphere && colB.shape == Core::ColliderShape::Sphere)
		{
			return TestSphereSphere(colA, transA, colB, transB, outNormal, outPenetration);
		}

		// 2. Box vs Box
		if (colA.shape == Core::ColliderShape::Box && colB.shape == Core::ColliderShape::Box)
		{
			OBB obbA = MakeOBB(transA, colA);
			OBB obbB = MakeOBB(transB, colB);
			return TestOBBOBB(obbA, obbB, outNormal, outPenetration);
		}

		// 3. Box vs Sphere
		if (colA.shape == Core::ColliderShape::Box && colB.shape == Core::ColliderShape::Sphere)
		{
			return TestBoxSphere(colA, transA, colB, transB, outNormal, outPenetration);
		}

		// 4. Sphere vs Box
		if (colA.shape == Core::ColliderShape::Sphere && colB.shape == Core::ColliderShape::Box)
		{
			if (TestBoxSphere(colB, transB, colA, transA, outNormal, outPenetration))
			{
				outNormal = -outNormal;
				return true;
			}
			return false;
		}

		// 5. Capsule vs Sphere
		if (colA.shape == Core::ColliderShape::Capsule && colB.shape == Core::ColliderShape::Sphere)
		{
			return TestCapsuleSphere(colA, transA, colB, transB, outNormal, outPenetration);
		}

		// 6. Sphere vs Capsule
		if (colA.shape == Core::ColliderShape::Sphere && colB.shape == Core::ColliderShape::Capsule)
		{
			if (TestCapsuleSphere(colB, transB, colA, transA, outNormal, outPenetration))
			{
				outNormal = -outNormal;
				return true;
			}
			return false;
		}

		// 7. Capsule vs Capsule
		if (colA.shape == Core::ColliderShape::Capsule && colB.shape == Core::ColliderShape::Capsule)
		{
			return TestCapsuleCapsule(colA, transA, colB, transB, outNormal, outPenetration);
		}

		// 8. Box vs Capsule
		if (colA.shape == Core::ColliderShape::Box && colB.shape == Core::ColliderShape::Capsule)
		{
			return TestBoxCapsule(colA, transA, colB, transB, outNormal, outPenetration);
		}

		// 9. Capsule vs Box
		if (colA.shape == Core::ColliderShape::Capsule && colB.shape == Core::ColliderShape::Box)
		{
			if (TestBoxCapsule(colB, transB, colA, transA, outNormal, outPenetration))
			{
				outNormal = -outNormal;
				return true;
			}
			return false;
		}

		// 10. Sphere vs TriangleMesh / ConvexMesh
		if (colA.shape == Core::ColliderShape::Sphere && (colB.shape == Core::ColliderShape::TriangleMesh || colB.shape == Core::ColliderShape::ConvexMesh))
		{
			return TestSphereTriangleMesh(colA, transA, colB, transB, outNormal, outPenetration);
		}
		if ((colA.shape == Core::ColliderShape::TriangleMesh || colA.shape == Core::ColliderShape::ConvexMesh) && colB.shape == Core::ColliderShape::Sphere)
		{
			if (TestSphereTriangleMesh(colB, transB, colA, transA, outNormal, outPenetration))
			{
				outNormal = -outNormal;
				return true;
			}
			return false;
		}

		// 11. Box vs TriangleMesh / ConvexMesh
		if (colA.shape == Core::ColliderShape::Box && (colB.shape == Core::ColliderShape::TriangleMesh || colB.shape == Core::ColliderShape::ConvexMesh))
		{
			return TestOBBTriangleMesh(colA, transA, colB, transB, outNormal, outPenetration);
		}
		if ((colA.shape == Core::ColliderShape::TriangleMesh || colA.shape == Core::ColliderShape::ConvexMesh) && colB.shape == Core::ColliderShape::Box)
		{
			if (TestOBBTriangleMesh(colB, transB, colA, transA, outNormal, outPenetration))
			{
				outNormal = -outNormal;
				return true;
			}
			return false;
		}
		return false;
	}

	bool RaycastCollider(
		const Core::ColliderComponent& col,
		const Core::TransformComponent& trans,
		const Vector3& origin,
		const Vector3& normalizedDir,
		float maxDist,
		float& outT,
		Vector3& outNormal
	)
	{
		if (col.shape == Core::ColliderShape::Sphere)
		{
			Vector3 m = origin - trans.position;
			float radiusSq = col.radius * col.radius;
			if (m.LengthSquared() <= radiusSq)
			{
				outT = 0.0f;
				outNormal = -normalizedDir;
				return true;
			}

			float b = m.Dot(normalizedDir);
			float c = m.Dot(m) - radiusSq;

			if (c > 0.0f && b > 0.0f)
			{
				return false;
			}

			float discr = b * b - c;
			if (discr < 0.0f)
			{
				return false;
			}

			float t = -b - std::sqrt(discr);
			if (t < 0.0f)
			{
				t = 0.0f;
			}

			if (t <= maxDist)
			{
				outT = t;
				Vector3 hitPoint = origin + normalizedDir * t;
				outNormal = hitPoint - trans.position;
				if (outNormal.LengthSquared() > 1e-6f)
					outNormal.Normalize();
				else
					outNormal = -normalizedDir;
				return true;
			}
			return false;
		}
		else if (col.shape == Core::ColliderShape::Box)
		{
			Quaternion invRot;
			trans.rotation.Inverse(invRot);
			Vector3 localOrigin = Vector3::Transform(origin - trans.position, invRot);
			Vector3 localDir = Vector3::Transform(normalizedDir, invRot);

			Vector3 halfSize = col.size * 0.5f;

			if (std::abs(localOrigin.x) <= halfSize.x &&
			    std::abs(localOrigin.y) <= halfSize.y &&
			    std::abs(localOrigin.z) <= halfSize.z)
			{
				outT = 0.0f;
				outNormal = -normalizedDir;
				return true;
			}

			Vector3 minPt = -halfSize;
			Vector3 maxPt = halfSize;

			float tmin = 0.0f;
			float tmax = maxDist;
			Vector3 hitNorm(0.0f, 1.0f, 0.0f);

			for (int i = 0; i < 3; ++i)
			{
				float origComp = (i == 0) ? localOrigin.x : (i == 1 ? localOrigin.y : localOrigin.z);
				float dirComp  = (i == 0) ? localDir.x    : (i == 1 ? localDir.y    : localDir.z);
				float minComp  = (i == 0) ? minPt.x       : (i == 1 ? minPt.y       : minPt.z);
				float maxComp  = (i == 0) ? maxPt.x       : (i == 1 ? maxPt.y       : maxPt.z);

				if (std::abs(dirComp) < 0.00001f)
				{
					if (origComp < minComp || origComp > maxComp)
					{
						return false;
					}
				}
				else
				{
					float ood = 1.0f / dirComp;
					float t1 = (minComp - origComp) * ood;
					float t2 = (maxComp - origComp) * ood;
					Vector3 n1 = (i == 0) ? Vector3(-1, 0, 0) : (i == 1 ? Vector3(0, -1, 0) : Vector3(0, 0, -1));

					if (t1 > t2)
					{
						std::swap(t1, t2);
						n1 = -n1;
					}

					if (t1 > tmin)
					{
						tmin = t1;
						hitNorm = n1;
					}
					tmax = std::min(tmax, t2);

					if (tmin > tmax)
					{
						return false;
					}
				}
			}

			if (tmin <= maxDist)
			{
				outT = tmin;
				outNormal = Vector3::Transform(hitNorm, trans.rotation);
				outNormal.Normalize();
				return true;
			}
			return false;
		}
		else if (col.shape == Core::ColliderShape::Capsule)
		{
			Quaternion invRot;
			trans.rotation.Inverse(invRot);
			Vector3 localOrigin = Vector3::Transform(origin - trans.position, invRot);
			Vector3 localDir = Vector3::Transform(normalizedDir, invRot);

			float r = col.radius;
			float halfSeg = std::max(0.0f, col.height * 0.5f - r);

			// Check if local origin is inside capsule
			float clampedY = std::clamp(localOrigin.y, -halfSeg, halfSeg);
			Vector3 segPt(0.0f, clampedY, 0.0f);
			if ((localOrigin - segPt).LengthSquared() <= r * r)
			{
				outT = 0.0f;
				outNormal = -normalizedDir;
				return true;
			}

			float closestT = maxDist;
			bool foundHit = false;
			Vector3 hitLocalNormal(0.0f, 1.0f, 0.0f);

			// 1. Cylinder body (if halfSeg > 0)
			if (halfSeg > 0.0f)
			{
				float A = localDir.x * localDir.x + localDir.z * localDir.z;
				float B = 2.0f * (localOrigin.x * localDir.x + localOrigin.z * localDir.z);
				float C = localOrigin.x * localOrigin.x + localOrigin.z * localOrigin.z - r * r;

				if (A > 1e-7f)
				{
					float discr = B * B - 4.0f * A * C;
					if (discr >= 0.0f)
					{
						float sqrtDiscr = std::sqrt(discr);
						float t0 = (-B - sqrtDiscr) / (2.0f * A);
						float t1 = (-B + sqrtDiscr) / (2.0f * A);

						if (t0 >= 0.0f && t0 < closestT)
						{
							float y = localOrigin.y + t0 * localDir.y;
							if (y >= -halfSeg && y <= halfSeg)
							{
								closestT = t0;
								foundHit = true;
								hitLocalNormal = Vector3(localOrigin.x + t0 * localDir.x, 0.0f, localOrigin.z + t0 * localDir.z);
								hitLocalNormal.Normalize();
							}
						}
						if (!foundHit && t1 >= 0.0f && t1 < closestT)
						{
							float y = localOrigin.y + t1 * localDir.y;
							if (y >= -halfSeg && y <= halfSeg)
							{
								closestT = t1;
								foundHit = true;
								hitLocalNormal = Vector3(localOrigin.x + t1 * localDir.x, 0.0f, localOrigin.z + t1 * localDir.z);
								hitLocalNormal.Normalize();
							}
						}
					}
				}
			}

			// 2. Top cap (sphere at (0, halfSeg, 0))
			Vector3 topCenter(0.0f, halfSeg, 0.0f);
			Vector3 mTop = localOrigin - topCenter;
			float bTop = mTop.Dot(localDir);
			float cTop = mTop.Dot(mTop) - r * r;
			float discrTop = bTop * bTop - cTop;
			if (discrTop >= 0.0f)
			{
				float tTop = -bTop - std::sqrt(discrTop);
				if (tTop >= 0.0f && tTop < closestT)
				{
					Vector3 pt = localOrigin + localDir * tTop;
					if (pt.y >= halfSeg - 1e-4f)
					{
						closestT = tTop;
						foundHit = true;
						hitLocalNormal = pt - topCenter;
						if (hitLocalNormal.LengthSquared() > 1e-6f)
							hitLocalNormal.Normalize();
						else
							hitLocalNormal = Vector3(0.0f, 1.0f, 0.0f);
					}
				}
			}

			// 3. Bottom cap (sphere at (0, -halfSeg, 0))
			Vector3 botCenter(0.0f, -halfSeg, 0.0f);
			Vector3 mBot = localOrigin - botCenter;
			float bBot = mBot.Dot(localDir);
			float cBot = mBot.Dot(mBot) - r * r;
			float discrBot = bBot * bBot - cBot;
			if (discrBot >= 0.0f)
			{
				float tBot = -bBot - std::sqrt(discrBot);
				if (tBot >= 0.0f && tBot < closestT)
				{
					Vector3 pt = localOrigin + localDir * tBot;
					if (pt.y <= -halfSeg + 1e-4f)
					{
						closestT = tBot;
						foundHit = true;
						hitLocalNormal = pt - botCenter;
						if (hitLocalNormal.LengthSquared() > 1e-6f)
							hitLocalNormal.Normalize();
						else
							hitLocalNormal = Vector3(0.0f, -1.0f, 0.0f);
					}
				}
			}

			if (foundHit && closestT <= maxDist)
			{
				outT = closestT;
				outNormal = Vector3::Transform(hitLocalNormal, trans.rotation);
				outNormal.Normalize();
				return true;
			}
			return false;
		}

		if (col.shape == Core::ColliderShape::TriangleMesh || col.shape == Core::ColliderShape::ConvexMesh)
		{
			if (!col.collisionMesh || !col.collisionMesh->bvh) return false;

			Matrix world = trans.getLocalMatrix();
			Matrix invWorld = world.Invert();

			Vector3 localOrigin = Vector3::Transform(origin, invWorld);
			Vector3 localDir = Vector3::TransformNormal(normalizedDir, invWorld);
			float localDirLen = localDir.Length();
			if (localDirLen < 1e-6f) return false;

			Vector3 normLocalDir = localDir / localDirLen;
			float localMaxDist = maxDist * localDirLen;

			float localT = 0.0f;
			Vector3 localNormal;
			uint32_t hitTri = 0;
			if (col.collisionMesh->bvh->raycast(localOrigin, normLocalDir, localMaxDist, localT, localNormal, hitTri))
			{
				outT = localT / localDirLen;
				Matrix invWorldTranspose = invWorld.Transpose();
				outNormal = Vector3::TransformNormal(localNormal, invWorldTranspose);
				outNormal.Normalize();
				return true;
			}
			return false;
		}
		return false;
	}
}
