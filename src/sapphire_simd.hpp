#ifndef __COSINEKITTY_SAPPHIRE_SIMD_HPP
#define __COSINEKITTY_SAPPHIRE_SIMD_HPP

#include <cmath>
#include <vector>

#ifdef NO_RACK_DEPENDENCY
/*
 * In the rack context this ifdef isn't needed; rack gives you simde for free
 * but if you want to use this module outside of rack (which we do) you need to
 * have a bring-your-own simde approach which will just use RACK SIMDE in rack builds
 */
#if defined(__arm64)
#define SIMDE_ENABLE_NATIVE_ALIASES
#include "simde/x86/sse4.2.h"
#else
#include <pmmintrin.h>
#endif

#else
#include "rack.hpp"
#endif

namespace Sapphire
{
    class PhysicsVector
    {
    public:
        union {
            __m128 v;
            float s[4];
        };

        PhysicsVector()
            : v(_mm_set1_ps(0.0f))
            {}

        PhysicsVector(float x)      // cppcheck-suppress [noExplicitConstructor]
            : v(_mm_set1_ps(x))
            {}

        explicit PhysicsVector(__m128 _v)
            : v(_v)
            {}

        PhysicsVector(float x1, float x2, float x3, float x4)
            : v(_mm_setr_ps(x1, x2, x3, x4))
            {}

        float& operator[](int i) { return s[i]; }
        const float& operator[](int i) const { return s[i]; }
        static PhysicsVector zero() { return PhysicsVector{}; }
    };

    inline PhysicsVector operator + (const PhysicsVector& a, const PhysicsVector& b)
    {
        return PhysicsVector(_mm_add_ps(a.v, b.v));
    }

    inline PhysicsVector operator - (const PhysicsVector& a, const PhysicsVector& b)
    {
        return PhysicsVector(_mm_sub_ps(a.v, b.v));
    }

    inline PhysicsVector operator * (const PhysicsVector& a, const PhysicsVector& b)
    {
        return PhysicsVector(_mm_mul_ps(a.v, b.v));
    }

    inline PhysicsVector& operator += (PhysicsVector& a, const PhysicsVector& b)
    {
        return a = a + b;
    }

    inline PhysicsVector& operator -= (PhysicsVector& a, const PhysicsVector& b)
    {
        return a = a - b;
    }

    inline PhysicsVector& operator *= (PhysicsVector& a, const PhysicsVector& b)
    {
        return a = a * b;
    }

    inline float Dot(const PhysicsVector &a, const PhysicsVector &b)
    {
        PhysicsVector c = a * b;
        return c.s[0] + c.s[1] + c.s[2] + c.s[3];
    }

    inline PhysicsVector Cross(const PhysicsVector &a, const PhysicsVector &b)
    {
        return PhysicsVector(
            a[1]*b[2] - a[2]*b[1],
            a[2]*b[0] - a[0]*b[2],
            a[0]*b[1] - a[1]*b[0],
            0.0f
        );
    }

    inline float Magnitude(const PhysicsVector &a)
    {
        return std::sqrt(Dot(a, a));
    }

    inline PhysicsVector Interpolate(
        float slider,
        const PhysicsVector &a,
        const PhysicsVector &b)
    {
        PhysicsVector c = (1.0-slider)*a + slider*b;

        // Normalize the length to the match the first vector `a`.
        return sqrt(Dot(a,a) / Dot(c,c)) * c;
    }

    using PhysicsVectorList = std::vector<PhysicsVector>;
}

#endif // __COSINEKITTY_SAPPHIRE_SIMD_HPP
