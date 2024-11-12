/*
    chaos.hpp  -  Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/

#pragma once

#include <cmath>
#include <algorithm>

namespace Sapphire
{
    const double CHAOS_AMPLITUDE = 5.0;  // the intended peak amplitude of output voltage

    inline double Remap(double v, double vmin, double vmax)
    {
        // Remaps v from the range [vmin, vmax] to [-CHAOS_AMPLITUDE, +CHAOS_AMPLITUDE].
        // But before we know the range, return the unmodified signal.
        if (vmax <= vmin)
            return v;

        // How far along the range is v in the range [vmin, vmax]?
        double r = (v - vmin) / (vmax - vmin);      // [0, 1]
        return CHAOS_AMPLITUDE * (2*r - 1);
    }


    struct SlopeVector
    {
        double mx;
        double my;
        double mz;

        SlopeVector(double _mx, double _my, double _mz)
            : mx(_mx)
            , my(_my)
            , mz(_mz)
            {}

        SlopeVector operator / (double denom) const
        {
            return SlopeVector(mx/denom, my/denom, mz/denom);
        }
    };


    inline double KnobValue(double knob, double lo, double hi)
    {
        // Converts a knob value that goes from [-1, +1]
        // into a linear range [lo, hi].
        return ((hi + lo) + (knob * (hi - lo))) / 2;
    }


    class ChaoticOscillator
    {
    protected:
        double knob = 0.0;
        int mode = 0;

        virtual SlopeVector slopes(double x, double y, double z) const = 0;

    private:
        const int max_iter = 2;

        const double max_dt;
        const double x0;
        const double y0;
        const double z0;

        const double xmin;
        const double xmax;
        const double ymin;
        const double ymax;
        const double zmin;
        const double zmax;

        double x1{};
        double y1{};
        double z1{};

        void step(double dt)
        {
            SlopeVector s = slopes(x1, y1, z1);
            double dx = dt * s.mx;
            double dy = dt * s.my;
            double dz = dt * s.mz;
            for (int iter = 0; iter < max_iter; ++iter)
            {
                double xm = x1 + dx/2;
                double ym = y1 + dy/2;
                double zm = z1 + dz/2;
                s = slopes(xm, ym, zm);
                dx = dt * s.mx;
                dy = dt * s.my;
                dz = dt * s.mz;
            }
            x1 += dx;
            y1 += dy;
            z1 += dz;
        }

    public:
        ChaoticOscillator(
            double _max_dt,
            double _x0, double _y0, double _z0,
            double _xmin, double _xmax,
            double _ymin, double _ymax,
            double _zmin, double _zmax
        )
            : max_dt(_max_dt)
            , x0(_x0)
            , y0(_y0)
            , z0(_z0)
            , xmin(_xmin)
            , xmax(_xmax)
            , ymin(_ymin)
            , ymax(_ymax)
            , zmin(_zmin)
            , zmax(_zmax)
        {
            initialize();
        }

        virtual ~ChaoticOscillator() {}

        void initialize()
        {
            x1 = x0;
            y1 = y0;
            z1 = z0;
            mode = 0;
        }

        void setKnob(double k)
        {
            // On Cardinal builds, one of the environments uses a compiler
            // option to convert `double` constants to `float`.
            // This causes a compiler error calling std::clamp()
            // unless we "lock in" the types.
            const double minKnob = -1;
            const double maxKnob = +1;
            knob = std::clamp(k, minKnob, maxKnob);
        }

        virtual int getModeCount() const
        {
            return 1;
        }

        virtual const char* getModeName(int m) const
        {
            return "Default";
        }

        int getMode() const
        {
            return mode;
        }

        int setMode(int m)
        {
            int count = getModeCount();
            if (count > 0)
                mode = std::clamp(m, 0, count-1);
            return mode;
        }

        virtual int getLegacyMode() const
        {
            return 0;
        }

        // Scaled values...
        double vx() const { return Remap(x1, xmin, xmax); }
        double vy() const { return Remap(y1, ymin, ymax); }
        double vz() const { return Remap(z1, zmin, zmax); }

        void update(double dt)
        {
            // If the derived class has informed us of a maximum stable time increment,
            // use oversampling to keep the actual time increment within that limit:
            // find the smallest positive integer n such that dt/n <= max_dt.
            const int n = (max_dt <= 0.0) ? 1 : static_cast<int>(std::ceil(dt / max_dt));
            const double et = dt / n;
            for (int i = 0; i < n; ++i)
                step(et);
        }
    };


    class Rucklidge : public ChaoticOscillator     // http://www.3d-meier.de/tut19/Seite17.html
    {
    protected:
        SlopeVector slopes(double x, double y, double z) const override
        {
            const double a = KnobValue(knob, 3.8, 6.7);
            return SlopeVector (
                -2*x + a*y - y*z,
                x,
                -z + y*y
            );
        }

    public:
        Rucklidge()
            : ChaoticOscillator(
                5.0e-04,
                0.788174, 0.522280, 1.250344,
                -10.15,  +10.17,
                 -5.570,  +5.565,
                  0.000, +15.387)
            {}
    };


    class Halvorsen : public ChaoticOscillator    // http://www.3d-meier.de/tut19/Seite13.html
    {
    protected:
        SlopeVector slopes(double x, double y, double z) const override
        {
            const double a0 = 1.4;
            const double aw = 0.21;
            const double a = KnobValue(knob, a0-aw, a0+aw);

            return SlopeVector(
                -a*x - 4*y - 4*z - y*y,
                -a*y - 4*z - 4*x - z*z,
                -a*z - 4*x - 4*y - x*x
            );
        }

    public:
        Halvorsen()
            : ChaoticOscillator(
                1.0e-3,
                1.0, +0.00152, -0.00163,
                0, 0,
                0, 0,
                0, 0)
            {}
    };


    class DequanLi : public ChaoticOscillator     // http://www.3d-meier.de/tut19/Seite9.html
    {
    protected:
        SlopeVector slopes(double x, double y, double z) const override
        {
            const double a0 = 40;
            const double aw = 6.15;
            const double a = (mode==0) ? KnobValue(knob, a0-aw, a0+aw) : a0;

            const double c = 1.833;
            const double d = 0.16;

            const double e0 = 0.644475;
            const double ew = 0.079475;
            const double e = (mode==1) ? KnobValue(knob, e0-ew, e0+ew) : e0;

            const double f0 = 20;
            const double fw = -3.9;
            const double f = (mode==2) ? KnobValue(knob, f0-fw, f0+fw) : f0;

            const double k0 = 55;
            const double kw = 20;
            const double k = (mode==3) ? KnobValue(knob, k0-kw, k0+kw) : k0;

            // The original Dequan Li attractor formulas result in a very
            // fast-moving particle, which is inconsistent with Frolic and Glee.
            // Decrease the speed to be more consistent.
            const double speedDenom = 30;

            return SlopeVector (
                a*(y-x) + d*x*z,
                k*x + f*y - x*z,
                c*z + x*y - e*x*x
            ) / speedDenom;
        }

    public:
        DequanLi()
            : ChaoticOscillator(
                1.0e-3,
                0.349, 0.001, -0.16,
                -150, +150,
                -200, +200,
                 -60, +300)
            {}

        int getModeCount() const override
        {
            return 4;
        }

        const char* getModeName(int m) const override
        {
            switch (m)
            {
            case 0:  return "Aardvark";
            case 1:  return "Elephant";
            case 2:  return "Ferret";
            case 3:  return "Kangaroo";
            default: return "INVALID_MODE";
            }
        }
    };


    class Aizawa : public ChaoticOscillator     // http://www.3d-meier.de/tut19/Seite3.html
    {
    private:
        const double aMin = 0.92;
        const double aMax = 1.03;

        const double bMin = 0.58;
        const double bMax = 0.70;

        const double cMin = 0.5941;
        const double cMax = 0.6117;

        // eMin, eMax are backwards on purpose.
        // When e=0.6 it produces minimal chaos because it's just a circle!
        const double eMin = 0.60;
        const double eMax = 0.20;

    protected:
        SlopeVector slopes(double x, double y, double z) const override
        {
            const double a = (mode==0) ? KnobValue(knob, aMin, aMax) : 0.95;
            const double b = (mode==1) ? KnobValue(knob, bMin, bMax) : 0.69535;
            const double c = (mode==2) ? KnobValue(knob, cMin, cMax) : 0.6029;
            const double d = 3.5;
            const double e = (mode==3) ? KnobValue(knob, eMin, eMax) : 0.25;
            const double f = 0.1;

            return SlopeVector(
                (z-b)*x - d*y,
                d*x + (z-b)*y,
                c + a*z - z*z*z/3 - (x*x + y*y)*(1 + e*z) + f*z*x*x*x
            );
        }

    public:
        Aizawa()
            : ChaoticOscillator(
                5.0e-04,
                0.440125, -0.781267, -0.277170,
                -1.51, +1.51,
                -1.46, +1.54,
                -0.39, +1.86)
            {}

        int getModeCount() const override
        {
            return 4;
        }

        const char* getModeName(int m) const override
        {
            switch (m)
            {
            case 0:  return "Apple";
            case 1:  return "Banana";
            case 2:  return "Cantaloupe";
            case 3:  return "Elderberry";
            default: return "INVALID_MODE";
            }
        }

        int getLegacyMode() const override
        {
            // The legacy mode is not the same as the default mode.
            // The default mode is always 0, but the legacy mode
            // is provided for backward compatibility of patches that
            // were created before chaos modes existed.
            // Sapphire Glee's previous behavior is what we now call
            // Cantaloupe mode, but it isn't very fun, so we made it
            // a nonzero (non-default) value. This way, when people create
            // new patches and use Glee in it, Glee will default to Apple mode,
            // which is far more fun.
            //
            // legacy mode = what we did before adding chaos modes
            // default mode = 0 = what we would like to do by default moving forward
            return 2;
        }
    };
}
