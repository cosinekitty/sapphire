/*
    chaos.hpp  -  Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/

#pragma once

#include <cmath>

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
        }

        void setKnob(double k)
        {
            // Enforce keeping the knob in the range [-1, 1].
            knob = std::max(-1.0, std::min(+1.0, k));
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


    class Aizawa : public ChaoticOscillator     // http://www.3d-meier.de/tut19/Seite3.html
    {
    private:
        const double a = 0.95;
        const double b = 0.69535;
        const double d = 3.5;
        const double e = 0.25;
        const double f = 0.1;

    protected:
        SlopeVector slopes(double x, double y, double z) const override
        {
            const double c = KnobValue(knob, 0.5941, 0.6117);
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
    };
}
