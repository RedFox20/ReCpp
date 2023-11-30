#pragma once
#include "minmax.h"
/**
 * Basic Common Math utils, Copyright (c) 2023 - Jorma Rebane
 * Distributed under MIT Software License
 */
namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////

    constexpr double PI     = 3.14159265358979323846264338327950288;
    constexpr float  PIf    = 3.14159265358979323846264338327950288f;
    constexpr double SQRT2  = 1.41421356237309504880;  // sqrt(2)
    constexpr float  SQRT2f = 1.41421356237309504880f; // sqrt(2)

    /** @return Radians from degrees */
    static constexpr float radf(float degrees)
    {
        return (degrees * rpp::PIf) / 180.0f; // rads=(degs*PI)/180
    }
    /** @return Radians from degrees */
    static constexpr double radf(double degrees)
    {
        return (degrees * rpp::PI) / 180.0; // rads=(degs*PI)/180
    }
    /** @return Degrees from radians */
    static constexpr float degf(float radians)
    {
        return radians * (180.0f / rpp::PIf); // degs=rads*(180/PI)
    }
    /** @return Degrees from radians */
    static constexpr double degf(double radians)
    {
        return radians * (180.0 / rpp::PI); // degs=rads*(180/PI)
    }

    /** @brief Clamps a value between:  min <= value <= max */
    template<class T> static constexpr T clamp(const T value, const T min, const T max) 
    {
        return value < min ? min : (value < max ? value : max);
    }

    /**
     * @brief Math utility, Linear Interpolation
     *        ex: lerp(0.5, 30.0, 60.0); ==> 45.0, because 0.5 is halfway between [30, 60]
     * @param position Multiplier value such as 0.5, 1.0 or 1.5
     * @param start Starting bound of the linear range
     * @param end Ending bound of the linear range
     */
    template<class T> static constexpr T lerp(const T position, const T start, const T end)
    {
        return start + (end-start)*position;
    }

    /**
     * @brief Math utility, inverse operation of Linear Interpolation
     *        ex:  lerpInverse(45.0, 30.0, 60.0); ==> 0.5, because 45 is halfway between [30, 60]
     * @param value Value somewhere between [start, end].
     *              !!Out of bounds values are not checked!! Use clamp(lerpInverse(min, max, value), 0, 1); instead
     * @param start Starting bound of the linear range
     * @param end Ending bound of the linear range
     * @return [0..1] if in bounds
     *         Less than 0 or Greater than 1 if out of bounds
     *         0 if invalid span (end-start)==0
     */
    template<class T> static constexpr T lerpInverse(const T value, const T start, const T end)
    {
        T span = (end - start);
        return span == 0 ? 0 : (value - start) / span;
    }

    ///////////////////////////////////////////////////////////////////////////////

    /** @return TRUE if abs(value) is very close to 0.0, epsilon controls the threshold */
    template<class T> static constexpr bool nearlyZero(const T& value, const T epsilon = (T)0.001)
    {
        return abs(value) <= epsilon;
    }

    /** @return TRUE if a and b are very close to being equal, epsilon controls the threshold */
    template<class T> static constexpr bool almostEqual(const T& a, const T& b, const T epsilon = (T)0.001)
    {
        return abs(a - b) <= epsilon;
    }

    ///////////////////////////////////////////////////////////////////////////////
}
