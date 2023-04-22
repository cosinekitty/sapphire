#ifndef __SAPPHIRE_CANTILEVER_HPP
#define __SAPPHIRE_CANTILEVER_HPP

#include <vector>
#include <cmath>

namespace Sapphire
{
    struct RodVector
    {
        float x;
        float y;

        RodVector(): x{0.0f}, y{0.0f} {}
        RodVector(float _x, float _y): x(_x), y(_y) {}

        RodVector operator + (const RodVector& other) const
        {
            return RodVector{x + other.x, y + other.y};
        }

        RodVector operator - (const RodVector& other) const
        {
            return RodVector{x - other.x, y - other.y};
        }

        RodVector operator - () const
        {
            return RodVector{-x, -y};
        }

        RodVector& operator += (const RodVector& other)
        {
            x += other.x;
            y += other.y;
            return *this;
        }

        RodVector& operator -= (const RodVector& other)
        {
            x -= other.x;
            y -= other.y;
            return *this;
        }

        RodVector operator / (float denom) const
        {
            return RodVector{x/denom, y/denom};
        }

        RodVector operator * (float scalar) const
        {
            return RodVector{scalar*x, scalar*y};
        }

        RodVector& operator *= (float scalar)
        {
            x *= scalar;
            y *= scalar;
            return *this;
        }

        float quadrature() const
        {
            return x*x + y*y;
        }

        float magnitude() const
        {
            return std::sqrt(quadrature());
        }

        RodVector rotate90() const
        {
            // Rotate the vector 90 degrees counterclockwise.
            // This is analogous to multiplying a complex number by i.
            return RodVector{-y, +x};
        }

        RodVector rotate(float degrees) const
        {
            float radians = degrees * (M_PI / 180.0);
            float c = std::cos(radians);
            float s = std::sin(radians);
            return RodVector{x*c - y*s, x*s + y*c};
        }
    };

    inline float dot(const RodVector& a, const RodVector& b)
    {
        return a.x*b.x + a.y*b.y;
    }

    inline float cross(const RodVector& a, const RodVector& b)
    {
        return a.x*b.y - a.y*b.x;
    }

    inline RodVector phasor(float radius, float angle)
    {
        return RodVector{radius * std::cos(angle), radius * std::sin(angle)};
    }

    inline float arcsine_approx(float x)
    {
        // Computes a fast approximation of arcsine.
        // Fairly accurate for values of |x| < 0.6.
        // Beyond that, accuracy suffers as |x| approaches 1.
        // This is good enough for my rotary torque model.
        return x*(1 + x*x/6);
    }

    struct Rod
    {
        float mass{};           // mass of rod's right endpoint in kilograms
        RodVector pos;          // location of the rod's right endpoint
        RodVector vel;          // velocity of the rod's right endpoint
    };


    class CantileverEngine
    {
    private:
        const float stretch = 2.0f;   // rod tensile stiffness [N/m]

        std::vector<RodVector> forceList;
        std::vector<Rod> nextRods;
        float bend{};           // joint rotary stiffness [N*m/rad]
        float tripFrac{};       // fraction of cantilever length at which we detect trip
        float tripY{};          // y-coordinate above which we trip the magnet
        float magnet{};         // magnetic field strength when active
        float halflife{};       // halflife of energy decay in seconds
        float massFactor{};     // dimensionless coefficient to multiply by ball masses
        bool isTripped{};       // is the magnet currently tripped?

        void initRods()
        {
            rods.clear();
            Rod rod;
            rod.mass = mass;
            for (int i = 0; i < nrods; ++i)
            {
                rod.pos = RodVector{restLength * (1+i), 0.0f};
                rods.push_back(rod);
            }
            nextRods = rods;
        }

    public:
        const int nrods;
        const float speedLimit = 10.0f;     // [m/s]
        const float restLength = 0.01f;     // [m]
        const float mass = 3.0e-8f;         // [kg]
        std::vector<Rod> rods;

        explicit CantileverEngine(int _nrods)
            : nrods(std::max(1, _nrods))
        {
        }

        void initialize()
        {
            initRods();
            setBend();
            setTripX();
            setTripY();
            setMagnet();
            setHalfLife();
            setMassFactor();
            isTripped = false;
        }

        void update(float dt)
        {
            dampen(dt);
            updateTrip();
            calcForces(rods);
            extrapolate(dt/2, speedLimit, massFactor, forceList, rods, nextRods);
            calcForces(nextRods);
            extrapolate(dt, speedLimit, massFactor, forceList, rods, nextRods);
            rods = nextRods;
        }

        void process(float dt, float sample[2])
        {
            update(dt);
            const float factor = 1000.0f;
            sample[0] = factor * ((rods[4].pos - rods[3].pos).magnitude() - restLength);
            sample[1] = factor * ((rods[8].pos - rods[7].pos).magnitude() - restLength);
        }

        void rotateRod(int index, float degrees)
        {
            if (index >= 0 && index < nrods)
            {
                RodVector dir = rods[index].pos;
                if (index > 0)
                    dir -= rods[index-1].pos;

                dir = dir.rotate(degrees);
                if (index > 0)
                    dir += rods[index-1].pos;

                rods[index].pos = dir;
            }
        }

        void setBend(float knob = 0.5f)
        {
            // The `knob` value is an externally facing dial value.
            // Map it to an internal exponential range.
            bend = 4.0e-4f * std::pow(10.0f, 2.0f*(knob - 0.5f));
        }

        void setTripX(float knob = 0.5f)
        {
            tripFrac = std::max(0.0f, std::min(1.0f, knob));
        }

        void setTripY(float knob = 0.5f)
        {
            tripY = (knob - 0.5f)*(restLength / 3.0f);
        }

        void setMagnet(float knob = 0.5f)
        {
            magnet = 1.0e-3f * std::pow(10.0f, 2.0f*(knob - 1.0f));
        }

        void setHalfLife(float knob = 0.5f)
        {
            halflife = std::pow(10.0f, 3.0f*(knob - 0.5f));
        }

        void setMassFactor(float knob = 0.5f)
        {
            massFactor = std::pow(10.0f, 2.0f*(knob - 0.5f));
        }

    private:
        void dampen(float dt)
        {
            // damp^(frictionHalfLife/dt) = 0.5.
            const float damp = pow(0.5, dt/halflife);
            for (Rod& r : rods)
                r.vel *= damp;
        }

        void updateTrip()
        {
            // Find the y-coordinate of the point along the cantilever
            // corresponding to the fraction of the cantilever's total length.
            // First of all, which rod are we talking about?
            float pos = tripFrac * nrods;
            int rindex = std::max(0, std::min(nrods-1, static_cast<int>(std::floor(pos))));

            // Secondly, how far along this rod are we talking about?
            float rfrac = std::max(0.0f, std::min(1.0f, pos - rindex));

            // Get the y-coordinates of this rod's endpoints.
            float y1 = (rindex > 0) ? rods.at(rindex-1).pos.y : 0.0f;
            float y2 = rods.at(rindex).pos.y;

            // Interpolate the y-coordinate at the selected location along the rod.
            float y = y1 + rfrac*(y2 - y1);

            // Should the magnet be turned on or off?
            isTripped = (y >= tripY);
        }

        static void extrapolate(
            float dt,
            float speedLimit,
            float massFactor,
            const std::vector<RodVector>& forceList,
            const std::vector<Rod>& sourceList,
            std::vector<Rod>& targetList)
        {
            const float speedLimitSquared = speedLimit * speedLimit;
            const int n = static_cast<int>(sourceList.size());
            for (int i = 0; i < n; ++i)
            {
                // F = ma  ==>  a = F/m   ==>  dv = dt*(F/m)
                RodVector dv = forceList[i] * (dt / (massFactor * sourceList[i].mass));

                // Assume the mean velocity over the interval is v + dv/2.
                // dr = dt*(v + dv/2).
                targetList[i].pos = sourceList[i].pos + (sourceList[i].vel + dv/2)*dt;
                targetList[i].vel = sourceList[i].vel + dv;

                float speedSquared = targetList[i].vel.quadrature();
                if (speedSquared > speedLimitSquared)
                    targetList[i].vel *= speedLimit / std::sqrt(speedSquared);
            }
        }

        void calcForces(const std::vector<Rod>& rodList)
        {
            forceList.resize(rodList.size());

            float prevLength = 0.0f;
            RodVector prevUnitDir{1.0f, 0.0f};
            const int n = static_cast<int>(rodList.size());
            for (int i = 0; i < n; ++i)
            {
                const Rod &r = rodList[i];

                // Calculate the length of this rod.
                RodVector dir = r.pos;
                if (i > 0)
                    dir -= rodList[i-1].pos;

                float length = dir.magnitude();
                RodVector udir = dir / length;      // unit vector

                // Tension force from stretching/compressing rod away from its rest length.
                // Tension is positive if the rod is stretched too long and trying to get shorter,
                // or negative if it is compressed and trying to get longer again.
                RodVector tension = udir * (stretch*(length - restLength));
                forceList[i] = -tension;

                if (isTripped && (i == n-1))
                {
                    // Include a downward magnetic force.
                    forceList[i].y -= magnet;
                }

                // Calculate torque caused by the bending of the left joint.
                // If this is the first rod, the bending is with respect to the x-direction.
                // Otherwise, bending is with respect to the previous rod.
                // Use the formula angle = arcsine(left x right).
                float torque = bend * arcsine_approx(cross(prevUnitDir, udir));

                // Positive torque tries to turn this rod clockwise (negative rotation).
                applyTorque(forceList, length, udir, i, -torque);

                if (i > 0)
                {
                    // Tension pulls the previous rod's right endpoint along this rod's direction.
                    forceList[i-1] += tension;

                    // Positive torque tries to turn the previous rod counterclockwise (positive rotation).
                    applyTorque(forceList, prevLength, prevUnitDir, i-1, +torque);
                }

                prevUnitDir = udir;
                prevLength = length;
            }
        }

        static void applyTorque(
            std::vector<RodVector>& forceList,
            float length,
            const RodVector& udir,
            int i,
            float torque)
        {
            // Apply torque by a force-couple applied to both endpoints of this rod.
            // The forces are at right angles to the rod's unit direction vector `udir`.
            RodVector perp = udir.rotate90();
            RodVector force = perp * (torque / length);
            forceList[i] += force;
            if (i > 0)
                forceList[i-1] -= force;
        }
    };
}

#endif // __SAPPHIRE_CANTILEVER_HPP