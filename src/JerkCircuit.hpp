/*
 *  JerkCircuit.hpp  -  Don Cross <cosinekitty@gmail.com>
 *
 *  A simulation of J. C. Sprott's chaotic "Jerk Circuit":
 *  https://sprott.physics.wisc.edu/pubs/paper352.pdf
 */

#pragma once

#include <cmath>

namespace Analog
{
    class JerkCircuit
    {
    private:
        const double maxDt = 2.3e-5;
        double timeDilation = 1.0;
        double timeDilationExponent = 0.0;      // cached for efficiency, to avoid redundant calls to std::pow()
        const double w0;    // initial voltage of capacitor C1
        const double x0;    // initial voltage of capacitor C2
        const double y0;    // initial voltage of capacitor C3

        //const double R1 = 1000;
        const double R2 = 1000;
        const double R3 = 1000;
        const double R4 = 1000;
        const double R5 = 1000;
        const double R6 = 1000;

        const double C1 = 1.0e-6;
        const double C2 = 1.0e-6;
        const double C3 = 1.0e-6;

        static double diodeCurrent(double voltage)
        {
            // don@doctorno:~/github/diodeplot$ ./run red_3
            // A=-26.714774051906932, B=112.53225596759907, C=-115.91134470261159
            const double A =  -26.714774051906932;
            const double B = +112.53225596759907;
            const double C = -115.91134470261159;
            double current = std::exp(A*voltage*voltage + B*voltage + C) - std::exp(C);
            return current;
        }

        // Node voltages
        double w1{};     // voltage at node  1
        double x1{};     // voltage at node  7
        double y1{};     // voltage at node  8
        double z1{};     // voltage at node 14

        // Node voltage increments
        double dw{};
        double dx{};
        double dy{};

        double rknob = 0.0;

        int step(double dt)
        {
            // Form an initial guess about the mean voltage during the time interval `dt`.
            // Use linear extrapolation to guess that the voltages will keep changing
            // at the same rate they did in the previous sample.
            double wm = w1 + dw/2;
            double xm = x1 + dx/2;
            double ym = y1 + dy/2;
            double zm = -(R6/R4)*xm;

            // Iterate until convergence.
            const double tolerance = 1.0e-12;        // one picovolt
            const double toleranceSquared = tolerance * tolerance;

            double r1 = 700.0 + (rknob*500.0);

            for (int iter = 1; true; ++iter)
            {
                // Remember the previous delta voltages, so we can tell whether we have converged next time.
                double ex = dx;
                double ew = dw;
                double ey = dy;

                // Update the finite changes of the voltage variables after the time interval.
                dw = -dt/C1*(wm/r1 + diodeCurrent(zm) + ym/R5);
                dx = -dt/C2*(wm/R2);
                dy = -dt/C3*(xm/R3);

                double w2 = w1 + dw;
                double x2 = x1 + dx;
                double y2 = y1 + dy;

                // Assume z changes instantaneously because there is no capacitor the feedback loop.
                double z2 = -(R6/R4)*x2;    // z changes instantly because there is no capacitor in the feedback path

                // Has the solver converged?
                // Calculate how much the deltas have changed since last time.
                double ddw = dw - ew;
                double ddx = dx - ex;
                double ddy = dy - ey;
                double variance = ddx*ddx + ddw*ddw + ddy*ddy;
                if (variance < toleranceSquared || iter >= iterationLimit)
                {
                    // The solution has converged, or we have hit the iteration safety limit.
                    // Update the circuit state voltages and return.
                    x1 = x2;
                    w1 = w2;
                    y1 = y2;
                    z1 = z2;
                    return iter;
                }

                // We approximate the mean value over the time interval as the average
                // of the starting value with the estimated next value.
                xm = x1 + dx/2;
                wm = w1 + dw/2;
                zm = (z1 + z2)/2;
            }
        }

    public:
        const int iterationLimit = 5;

        JerkCircuit(double _w0, double _x0, double _y0)
            : w0(_w0)
            , x0(_x0)
            , y0(_y0)
        {
            initialize();
        }

        void initialize()
        {
            w1 = w0;
            x1 = x0;
            y1 = y0;
            z1 = -(R6/R4)*x0;       // op-amp acts instantly
            dw = dx = dy = 0;
        }

        int update(float sampleRateHz)
        {
            double dt = timeDilation / sampleRateHz;
            int nsteps = static_cast<int>(std::ceil(dt / maxDt));
            dt /= nsteps;
            int iter = 0;
            for (int s = 0; s < nsteps; ++s)
                iter += step(dt);
            return iter;
        }

        void setTimeDilationExponent(double exponent)
        {
            if (exponent != timeDilationExponent)
            {
                timeDilationExponent = exponent;
                timeDilation = std::pow(10.0, exponent);
            }
        }

        void setResistanceKnob(double knob)
        {
            rknob = std::max(-1.0, std::min(+1.0, knob));
        }

        double wVoltage() const { return w1; }
        double xVoltage() const { return x1; }
        double yVoltage() const { return y1; }
        double zVoltage() const { return z1; }
    };
}
