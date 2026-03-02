#pragma once

#include <concepts>
#include <vector>

namespace basis
{
	template<typename T, typename POINT_T>
	concept SplineInterpolator = requires(const POINT_T & p0, const POINT_T& p1, const POINT_T& p2, const POINT_T& p3, float t)
	{
		{ T::Interpolate(p0, p1, p2, p3, t) } -> std::convertible_to<POINT_T>;
	};

	template<typename T, SplineInterpolator<T> INTERPOLATOR>
	class Spline
	{
		std::vector<T> m_points;

	public:
		Spline(std::initializer_list<T> points)
		{
			for (auto& p : points)
				AddPoint(p);
		}

		void AddPoint(const T& point)
		{
			m_points.push_back(point);
		}

		T At(float t)
		{
			if (m_points.empty())
				return T(0.0f);

			if (m_points.size() <= 2)
				return m_points[0];

			if (t < 1.0f)
				return m_points[1];

			if (t >= m_points.size() - 2)
				return m_points[m_points.size() - 2];

			uint32_t p1 = uint32_t(std::floor(t));
			uint32_t p2 = uint32_t(std::ceil(t));
			uint32_t p0 = p1 - 1;
			uint32_t p3 = p2 + 1;
			float tLocal = t - std::floor(t);

			return INTERPOLATOR::Interpolate(m_points[p0], m_points[p1], m_points[p2], m_points[p3], tLocal);
		}
	};

	template<typename T>
	struct CatmullRomInterpolator
	{
		CatmullRomInterpolator() = delete;
		static T Interpolate(const T& p0, const T& p1, const T& p2, const T& p3, float t)
		{
			T m1 = 0.5f * (p2 - p0);
			T m2 = 0.5f * (p3 - p1);

			float h00 = 2.0f * t * t * t - 3.0f * t * t + 1.0f;
			float h10 = t * t * t - 2.0f * t * t + t;
			float h01 = -2.0f * t * t * t + 3.0f * t * t;
			float h11 = t * t * t - t * t;

			return h00 * p1 + h10 * m1 + h01 * p2 + h11 * m2;
		}
	};

	template<typename T>
	using CatmullRomSpline = Spline<T, CatmullRomInterpolator<T>>;
}
