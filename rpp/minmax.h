#pragma once

// You can disable RPP SSE intrinsics by declaring 
// #define RPP_SSE_INTRINSICS 0 before including <rpp/minmax.h>
#ifndef RPP_SSE_INTRINSICS
#  if _M_IX86_FP || _M_AMD64 || _M_X64 || __SSE2__
#    define RPP_SSE_INTRINSICS 1
#  endif
#endif

#if RPP_SSE_INTRINSICS
#  include <emmintrin.h>
#endif
#include <math.h>

#undef min
#undef max

namespace rpp
{
#if RPP_SSE_INTRINSICS
    inline float min(const float& a, const float& b) noexcept
    {
        return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(a), _mm_set_ss(b)));
    }
    inline double min(const double& a, const double& b) noexcept
    {
        return _mm_cvtsd_f64(_mm_min_sd(_mm_set_sd(a), _mm_set_sd(b)));
    }

    inline float max(const float& a, const float& b) noexcept
    {
        return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(a), _mm_set_ss(b)));
    }
    inline double max(const double& a, const double& b) noexcept
    {
        return _mm_cvtsd_f64(_mm_max_sd(_mm_set_sd(a), _mm_set_sd(b)));
    }

    inline float abs(const float& a) noexcept
    {
        return _mm_cvtss_f32(_mm_andnot_ps(_mm_castsi128_ps(_mm_set1_epi32(0x80000000)), _mm_set_ss(a)));
    }
    inline double abs(const double& a) noexcept
    {
        return _mm_cvtsd_f64(_mm_andnot_pd(_mm_castsi128_pd(_mm_set1_epi64x(0x8000000000000000UL)), _mm_set_sd(a)));
    }

    inline float sqrt(const float& f) noexcept
    {
        return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(f)));
    }
    inline double sqrt(const double& d) noexcept
    {
        auto m = _mm_set_sd(d); return _mm_cvtsd_f64(_mm_sqrt_sd(m, m));
    }
#else
    inline float abs(const float& a)   noexcept { return ::fabs(a); }
    inline double abs(const double& a) noexcept { return ::fabs(a); }

    inline float sqrt(const float& f)   noexcept { return ::sqrtf(f); }
    inline double sqrt(const double& f) noexcept { return ::sqrt(f);  }
#endif

    template<class T> static constexpr const T& min(const T& a, const T& b) { return a < b ?  a : b; }
    template<class T> static constexpr const T& max(const T& a, const T& b) { return a > b ?  a : b; }
    template<class T> static constexpr T abs(const T& a)                    { return a < 0 ? -a : a; }

    template<class T> static constexpr const T& min3(const T& a, const T& b, const T& c)
    {
        return a < b ? (a<c ? a : c) : (b<c ? b : c);
    }
    template<class T> static constexpr const T& max3(const T& a, const T& b, const T& c)
    {
        return a > b ? (a>c ? a : c) : (b>c ? b : c);
    }

} // rpp
