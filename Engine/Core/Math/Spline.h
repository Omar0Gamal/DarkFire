#pragma once

#include "Engine/Core/Math/Math.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>

namespace Core::Math
{
	inline Vector3 evaluateCatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) noexcept
	{
		const float t2 = t * t;
		const float t3 = t2 * t;

		return 0.5f * ((2.0f * p1) +
			(-p0 + p2) * t +
			(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
			(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
	}

	inline Vector3 evaluateCatmullRomTangent(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) noexcept
	{
		const float t2 = t * t;

		return 0.5f * ((-p0 + p2) +
			2.0f * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t +
			3.0f * (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t2);
	}

	namespace Internal
	{
		inline void getSplineSegmentPoints(const std::vector<Vector3>& points, bool isClosedLoop, int segment,
			Vector3& outP0, Vector3& outP1, Vector3& outP2, Vector3& outP3) noexcept
		{
			const int n = static_cast<int>(points.size());
			if (n == 0) return;

			if (isClosedLoop)
			{
				outP0 = points[(segment - 1 + n) % n];
				outP1 = points[segment % n];
				outP2 = points[(segment + 1) % n];
				outP3 = points[(segment + 2) % n];
			}
			else
			{
				outP1 = points[segment];
				outP2 = points[segment + 1];
				outP0 = (segment > 0) ? points[segment - 1] : (2.0f * outP1 - outP2);
				outP3 = (segment + 2 < n) ? points[segment + 2] : (2.0f * outP2 - outP1);
			}
		}

		inline void getSplineSegmentAndLocalT(size_t pointCount, bool isClosedLoop, float normalizedT,
			int& outSegment, float& outLocalT, int& outNumSegments) noexcept
		{
			const float clampedT = std::clamp(normalizedT, 0.0f, 1.0f);
			outNumSegments = isClosedLoop ? static_cast<int>(pointCount) : static_cast<int>(pointCount - 1);

			if (outNumSegments <= 0)
			{
				outSegment = 0;
				outLocalT = 0.0f;
				return;
			}

			if (clampedT >= 1.0f)
			{
				outSegment = outNumSegments - 1;
				outLocalT = 1.0f;
				return;
			}

			const float scaledT = clampedT * static_cast<float>(outNumSegments);
			outSegment = static_cast<int>(scaledT);
			if (outSegment >= outNumSegments)
			{
				outSegment = outNumSegments - 1;
			}
			outLocalT = scaledT - static_cast<float>(outSegment);
		}
	}

	inline Vector3 evaluateSplinePosition(const std::vector<Vector3>& points, bool isClosedLoop, float normalizedT) noexcept
	{
		if (points.empty())
		{
			return Vector3::Zero;
		}
		if (points.size() == 1)
		{
			return points[0];
		}
		if (!isClosedLoop && points.size() == 2)
		{
			const float t = std::clamp(normalizedT, 0.0f, 1.0f);
			return Vector3::Lerp(points[0], points[1], t);
		}

		int segment = 0;
		float localT = 0.0f;
		int numSegments = 0;
		Internal::getSplineSegmentAndLocalT(points.size(), isClosedLoop, normalizedT, segment, localT, numSegments);

		Vector3 p0, p1, p2, p3;
		Internal::getSplineSegmentPoints(points, isClosedLoop, segment, p0, p1, p2, p3);

		return evaluateCatmullRom(p0, p1, p2, p3, localT);
	}

	inline Vector3 evaluateSplineTangent(const std::vector<Vector3>& points, bool isClosedLoop, float normalizedT) noexcept
	{
		if (points.size() < 2)
		{
			return Vector3(0.0f, 0.0f, 1.0f);
		}
		if (!isClosedLoop && points.size() == 2)
		{
			Vector3 dir = points[1] - points[0];
			const float lenSq = dir.LengthSquared();
			if (lenSq > 1e-8f)
			{
				dir.Normalize();
			}
			return dir;
		}

		int segment = 0;
		float localT = 0.0f;
		int numSegments = 0;
		Internal::getSplineSegmentAndLocalT(points.size(), isClosedLoop, normalizedT, segment, localT, numSegments);

		Vector3 p0, p1, p2, p3;
		Internal::getSplineSegmentPoints(points, isClosedLoop, segment, p0, p1, p2, p3);

		Vector3 tangent = evaluateCatmullRomTangent(p0, p1, p2, p3, localT);
		const float lenSq = tangent.LengthSquared();
		if (lenSq > 1e-8f)
		{
			tangent.Normalize();
		}
		return tangent;
	}

	inline float computeSplineArcLength(const std::vector<Vector3>& points, bool isClosedLoop, int samplesPerSegment = 16) noexcept
	{
		if (points.size() < 2)
		{
			return 0.0f;
		}

		const int numSegments = isClosedLoop ? static_cast<int>(points.size()) : static_cast<int>(points.size() - 1);
		if (numSegments <= 0)
		{
			return 0.0f;
		}

		const int samples = std::max(1, samplesPerSegment);
		float totalLength = 0.0f;

		for (int seg = 0; seg < numSegments; ++seg)
		{
			Vector3 p0, p1, p2, p3;
			Internal::getSplineSegmentPoints(points, isClosedLoop, seg, p0, p1, p2, p3);

			Vector3 prevPos = p1;
			for (int i = 1; i <= samples; ++i)
			{
				const float t = static_cast<float>(i) / static_cast<float>(samples);
				const Vector3 currPos = evaluateCatmullRom(p0, p1, p2, p3, t);
				totalLength += Vector3::Distance(prevPos, currPos);
				prevPos = currPos;
			}
		}

		return totalLength;
	}

	inline Vector3 findClosestPointOnSpline(const std::vector<Vector3>& points, bool isClosedLoop, const Vector3& worldPos, float& outNormalizedT, int samplesPerSegment = 16) noexcept
	{
		if (points.empty())
		{
			outNormalizedT = 0.0f;
			return Vector3::Zero;
		}
		if (points.size() == 1)
		{
			outNormalizedT = 0.0f;
			return points[0];
		}

		const int numSegments = isClosedLoop ? static_cast<int>(points.size()) : static_cast<int>(points.size() - 1);
		if (numSegments <= 0)
		{
			outNormalizedT = 0.0f;
			return points[0];
		}

		const int samples = std::max(2, samplesPerSegment);
		const int totalSamples = numSegments * samples;
		float bestDistSq = (std::numeric_limits<float>::max)();
		float bestT = 0.0f;
		Vector3 bestPoint = points[0];

		// Coarse sampling
		for (int i = 0; i <= totalSamples; ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(totalSamples);
			const Vector3 pos = evaluateSplinePosition(points, isClosedLoop, t);
			const float d2 = Vector3::DistanceSquared(pos, worldPos);
			if (d2 < bestDistSq)
			{
				bestDistSq = d2;
				bestT = t;
				bestPoint = pos;
			}
		}

		// Refinement around bestT with ternary search
		const float deltaT = 1.0f / static_cast<float>(totalSamples);
		float lowT = std::max(0.0f, bestT - deltaT);
		float highT = std::min(1.0f, bestT + deltaT);

		for (int iter = 0; iter < 10; ++iter)
		{
			const float mid1 = lowT + (highT - lowT) / 3.0f;
			const float mid2 = highT - (highT - lowT) / 3.0f;

			const Vector3 pos1 = evaluateSplinePosition(points, isClosedLoop, mid1);
			const Vector3 pos2 = evaluateSplinePosition(points, isClosedLoop, mid2);

			const float dist1 = Vector3::DistanceSquared(pos1, worldPos);
			const float dist2 = Vector3::DistanceSquared(pos2, worldPos);

			if (dist1 < dist2)
			{
				highT = mid2;
				if (dist1 < bestDistSq)
				{
					bestDistSq = dist1;
					bestT = mid1;
					bestPoint = pos1;
				}
			}
			else
			{
				lowT = mid1;
				if (dist2 < bestDistSq)
				{
					bestDistSq = dist2;
					bestT = mid2;
					bestPoint = pos2;
				}
			}
		}

		outNormalizedT = bestT;
		return bestPoint;
	}
}
