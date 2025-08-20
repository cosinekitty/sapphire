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

    inline double Remap(double s, double smin, double smax)
    {
        // Remaps s from the range [smin, smax] to [-CHAOS_AMPLITUDE, +CHAOS_AMPLITUDE].
        // But before we know the range, return the unmodified signal.
        if (smax <= smin)
            return s;

        // How far along the range is s in the range [smin, smax]?
        double r = (s - smin) / (smax - smin);      // [0, 1]
        return CHAOS_AMPLITUDE * (2*r - 1);
    }


    struct SlopeVector
    {
        double mx;
        double my;
        double mz;

        explicit SlopeVector()
            : mx(0)
            , my(0)
            , mz(0)
            {}

        explicit SlopeVector(double _mx, double _my, double _mz)
            : mx(_mx)
            , my(_my)
            , mz(_mz)
            {}

        SlopeVector operator / (double denom) const
        {
            return SlopeVector(mx/denom, my/denom, mz/denom);
        }
    };


    inline SlopeVector operator * (double factor, const SlopeVector& vec)
    {
        return SlopeVector(factor*vec.mx, factor*vec.my, factor*vec.mz);
    }


    inline double KnobValue(double knob, double lo, double hi)
    {
        // Converts a knob value that goes from [-1, +1]
        // into a linear range [lo, hi].
        return ((hi + lo) + (knob * (hi - lo))) / 2;
    }


    struct ChaoticOscillatorState
    {
        // The internal state of a chaotic oscillator, provided for memory store/recall.
        // This is NOT the same as the voltage outputs because of remapping
        // the original numeric ranges and voltage-flipped output ports.
        double x;
        double y;
        double z;

        ChaoticOscillatorState()
            : x(0)
            , y(0)
            , z(0)
            {}

        explicit ChaoticOscillatorState(double _x, double _y, double _z)
            : x(_x)
            , y(_y)
            , z(_z)
            {}
    };


    class ChaoticOscillator
    {
    protected:
        double knob = 0.0;
        int mode = 0;

        virtual SlopeVector slopes(double x, double y, double z) const = 0;

        inline SlopeVector vel(double x, double y, double z) const
        {
            return speedFactor * slopes(x, y, z);
        }

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

        double speedFactor = 1;
        double dilate = 1;
        double xTranslate = 0;
        double yTranslate = 0;
        double zTranslate = 0;

        double xVelScale{};
        double yVelScale{};
        double zVelScale{};

    private:
        double x1{};
        double y1{};
        double z1{};

        void step(double dt)
        {
            // Fourth-order Runge-Kutta (RK4) extrapolation.
            SlopeVector k1 = vel(x1, y1, z1);
            SlopeVector k2 = vel(x1 + (dt/2)*k1.mx, y1 + (dt/2)*k1.my, z1 + (dt/2)*k1.mz);
            SlopeVector k3 = vel(x1 + (dt/2)*k2.mx, y1 + (dt/2)*k2.my, z1 + (dt/2)*k2.mz);
            SlopeVector k4 = vel(x1 + dt*k3.mx, y1 + dt*k3.my, z1 + dt*k3.mz);
            x1 += (dt/6)*(k1.mx + 2*k2.mx + 2*k3.mx + k4.mx);
            y1 += (dt/6)*(k1.my + 2*k2.my + 2*k3.my + k4.my);
            z1 += (dt/6)*(k1.mz + 2*k2.mz + 2*k3.mz + k4.mz);
        }

    public:
        virtual ~ChaoticOscillator() {}

        explicit ChaoticOscillator(
            double _max_dt,
            double _x0, double _y0, double _z0,
            double _xmin, double _xmax,
            double _ymin, double _ymax,
            double _zmin, double _zmax,
            double _xVelScale, double _yVelScale, double _zVelScale
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
            , xVelScale(_xVelScale)
            , yVelScale(_yVelScale)
            , zVelScale(_zVelScale)
        {
            ChaoticOscillator_initialize();
        }

        virtual void initialize()
        {
            ChaoticOscillator_initialize();
        }

        void ChaoticOscillator_initialize()
        {
            x1 = x0;
            y1 = y0;
            z1 = z0;
            mode = 0;
            speedFactor = 1;
            dilate = 1;
            xTranslate = yTranslate = zTranslate = 0;
        }

        void setKnob(double k)
        {
            knob = std::clamp<double>(k, -1.0, +1.0);
        }

        virtual int getModeCount() const
        {
            return 1;
        }

        virtual const char* getModeName(int m) const
        {
            return "";
        }

        virtual bool isModeEnabled(int mode) const
        {
            return (mode >= 0) && (mode < getModeCount());
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

        double getDilate() const
        {
            return dilate;
        }

        double setDilate(double _dilate = 1)
        {
            if (std::isfinite(_dilate))
                dilate = std::clamp<double>(_dilate, 0.01, 100.0);
            else
                dilate = 1;
            return dilate;
        }

        double getSpeedFactor() const
        {
            return speedFactor;
        }

        double setSpeedFactor(double _speedFactor = 1)
        {
            if (std::isfinite(_speedFactor))
                speedFactor = std::clamp<double>(_speedFactor, 0.01, 100.0);
            else
                speedFactor = 1;
            return speedFactor;
        }

        SlopeVector getTranslate() const
        {
            return SlopeVector(xTranslate, yTranslate, zTranslate);
        }

        void setTranslate(const SlopeVector& translate)
        {
            xTranslate = translate.mx;
            yTranslate = translate.my;
            zTranslate = translate.mz;
        }

        SlopeVector getMorphFactors() const
        {
            return SlopeVector(xVelScale, yVelScale, zVelScale);
        }

        void setMorphFactors(const SlopeVector& mf)
        {
            xVelScale = mf.mx;
            yVelScale = mf.my;
            zVelScale = mf.mz;
        }

        // Scaled position values.
        double xpos() const { return dilate * (xTranslate + Remap(x1, xmin, xmax)); }
        double ypos() const { return dilate * (yTranslate + Remap(y1, ymin, ymax)); }
        double zpos() const { return dilate * (zTranslate + Remap(z1, zmin, zmax)); }

        SlopeVector velocity() const
        {
            SlopeVector vec = vel(x1, y1, z1);
            vec.mx *= xVelScale;
            vec.my *= yVelScale;
            vec.mz *= zVelScale;
            return vec;
        }

        virtual void updateParameters() {}

        void update(double dt, int oversampling)
        {
            using namespace std;

            // If the derived class has informed us of a maximum stable time increment,
            // use oversampling to keep the actual time increment within that limit:
            // find the smallest positive integer n such that dt/n <= max_dt.
            const int rawCount = (max_dt <= 0.0) ? 1 : static_cast<int>(ceil(abs(dt) / max_dt));
            const int n = rawCount * std::max(1, oversampling);
            const double et = dt / n;
            for (int i = 0; i < n; ++i)
                step(et);
        }

        ChaoticOscillatorState getState() const
        {
            return ChaoticOscillatorState(x1, y1, z1);
        }

        void setState(const ChaoticOscillatorState& state)
        {
            x1 = state.x;
            y1 = state.y;
            z1 = state.z;
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
                0.03,
                0.788174, 0.522280, 1.250344,
                -10.15,  +10.17,
                 -5.570,  +5.565,
                  0.000, +15.387,
                0.2, 0.2, 0.2)
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
                0.03,
                0.349, 0.001, -0.16,
                -150, +150,
                -200, +200,
                 -60, +300,
                0.015, 0.0075, 0.0075)
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
            default: return "";
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
                0.03,
                0.440125, -0.781267, -0.277170,
                -1.51, +1.51,
                -1.46, +1.54,
                -0.39, +1.86,
                1, 1, 3.5)
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
            default: return "";
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
