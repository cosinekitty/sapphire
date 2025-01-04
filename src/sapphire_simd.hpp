#pragma once
#include <cmath>
#include <complex>
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
    using complex_t = std::complex<float>;

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

        bool isFinite3d() const
        {
            // Check for finite 3D vector, optimized by ignoring the fourth component that we don't care about.
            return std::isfinite(s[0]) && std::isfinite(s[1]) && std::isfinite(s[2]);
        }
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

    inline PhysicsVector operator + (const PhysicsVector& a)
    {
        return a;
    }

    inline PhysicsVector operator - (const PhysicsVector& a)
    {
        return PhysicsVector{-1} * a;
    }

    inline PhysicsVector operator / (const PhysicsVector& a, const PhysicsVector& b)
    {
        return PhysicsVector(_mm_div_ps(a.v, b.v));
    }

    inline PhysicsVector& operator += (PhysicsVector& a, const PhysicsVector& b)
    {
        a = a + b;
        return a;
    }

    inline PhysicsVector& operator -= (PhysicsVector& a, const PhysicsVector& b)
    {
        a = a - b;
        return a;
    }

    inline PhysicsVector& operator *= (PhysicsVector& a, const PhysicsVector& b)
    {
        a = a * b;
        return a;
    }

    inline PhysicsVector& operator /= (PhysicsVector& a, float b)
    {
        a = a / b;
        return a;
    }

    inline float Dot(const PhysicsVector &a, const PhysicsVector &b)
    {
        PhysicsVector c = a * b;
        return c.s[0] + c.s[1] + c.s[2];
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

    inline float Quadrature(const PhysicsVector& a)
    {
        return Dot(a, a);
    }

    inline float Magnitude(const PhysicsVector &a)
    {
        return std::sqrt(Quadrature(a));
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

    inline PhysicsVector PivotAxis(float C, float S)
    {
        const float A = (1 - C) / 3;
        const float three = 3;  // explicitly state type `float` to sidestep optimizations in Cardinal/ARM
        const float B = S / std::sqrt(three);
        return PhysicsVector{C+A, A+B, A-B, 0};
    }

    // PivotAxis returns a unit vector.
    // If `steps` has an integer value, the vector points in
    // the direction of the positive x, y, or z axis, depending on
    // the mod 3.0 value of `steps`:
    //
    //      0.0 => [1, 0, 0] = x-axis
    //      1.0 => [0, 1, 0] = y-axis
    //      2.0 => [0, 0, 1] = z-axis
    //      3.0 => [1, 0, 0] = x-axis (again, etc)
    //
    // Sweeping the real value of `steps` creates a cone shape
    // that periodically passes through all 3 axes.
    // The value of `steps` may be negative or positive, and may
    // increase within the bounds of type `float`.
    // However, it is recommended to keep the values as close to
    // zero as is practical for your application, to reduce error in trig functions.
    inline PhysicsVector PivotAxis(float steps)
    {
        const float radians = ((2*M_PI)/3) * steps;
        const float C = std::cos(radians);
        const float S = std::sin(radians);
        return PivotAxis(C, S);
    }

    struct RotationMatrix
    {
        PhysicsVector xAxis;
        PhysicsVector yAxis;
        PhysicsVector zAxis;

        explicit RotationMatrix(PhysicsVector x, PhysicsVector y, PhysicsVector z)
            : xAxis(x)
            , yAxis(y)
            , zAxis(z)
            {}
    };

    inline RotationMatrix PivotAxes(float steps)
    {
        PhysicsVector x = PivotAxis(steps + 0);
        PhysicsVector y = PivotAxis(steps + 1);
        PhysicsVector z = PivotAxis(steps + 2);
        return RotationMatrix {
            PhysicsVector{x[0], y[0], z[0], 0},   // rotated x-axis unit vector
            PhysicsVector{x[1], y[1], z[1], 0},   // rotated y-axis unit vector
            PhysicsVector{x[2], y[2], z[2], 0}    // rotated z-axis unit vector
        };
    }

    using PhysicsVectorList = std::vector<PhysicsVector>;
}
