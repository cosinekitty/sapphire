#pragma once

#include <exception>
#include <string>
#include <vector>
#include <jansson.h>

namespace Sapphire
{
    struct PhysicsVector
    {
        double x;
        double y;
        double z;

        PhysicsVector()
            : x(0.0)
            , y(0.0)
            , z(0.0)
            {}

        PhysicsVector(double _x, double _y, double _z)
            : x(_x)
            , y(_y)
            , z(_z)
            {}

        PhysicsVector operator - ()
        {
            return PhysicsVector(-x, -y, -z);
        }

        PhysicsVector& operator += (const PhysicsVector& other)
        {
            x += other.x;
            y += other.y;
            z += other.z;
            return *this;
        }

        PhysicsVector& operator -= (const PhysicsVector& other)
        {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            return *this;
        }

        PhysicsVector& operator *= (const double scalar)
        {
            x *= scalar;
            y *= scalar;
            z *= scalar;
            return *this;
        }

        double MagnitudeSquared() const
        {
            return x*x + y*y + z*z;
        }

        double Magnitude() const;
    };

    inline PhysicsVector operator + (const PhysicsVector &a, const PhysicsVector &b)
    {
        return PhysicsVector(a.x+b.x, a.y+b.y, a.z+b.z);
    }

    inline PhysicsVector operator - (const PhysicsVector &a, const PhysicsVector &b)
    {
        return PhysicsVector(a.x-b.x, a.y-b.y, a.z-b.z);
    }

    inline PhysicsVector operator / (const PhysicsVector &v, double d)
    {
        return PhysicsVector(v.x/d, v.y/d, v.z/d);
    }

    inline PhysicsVector operator * (double s, const PhysicsVector &v)
    {
        return PhysicsVector(s*v.x, s*v.y, s*v.z);
    }

    inline double Dot(const PhysicsVector &a, const PhysicsVector &b)
    {
        return a.x*b.x + a.y*b.y + a.z*b.z;
    }

    struct Spring
    {
        int ballIndex1;         // 0-based index into Mesh::ballList
        int ballIndex2;         // 0-based index into Mesh::ballList

        Spring(int _ballIndex1, int _ballIndex2)
            : ballIndex1(_ballIndex1)
            , ballIndex2(_ballIndex2)
            {}
    };

    struct Ball
    {
        PhysicsVector pos;      // the ball's position [m]
        PhysicsVector vel;      // the ball's velocity [m/s]
        double mass;            // the ball's [kg]

        Ball(double _mass, double _x, double _y, double _z)
            : pos(_x, _y, _z)
            , vel()
            , mass(_mass)
            {}

        Ball(double _mass, PhysicsVector _pos, PhysicsVector _vel)
            : pos(_pos)
            , vel(_vel)
            , mass(_mass)
            {}

        static Ball Anchor(double _x, double _y, double _z)
        {
            return Ball(-1.0, _x, _y, _z);
        }

        bool IsAnchor() const { return mass <= 0.0; }
        bool IsMobile() const { return mass > 0.0; }
    };

    typedef std::vector<Spring> SpringList;
    typedef std::vector<Ball> BallList;
    typedef std::vector<PhysicsVector> PhysicsVectorList;
    typedef std::vector<double> PhysicsScalarList;


    class PhysicsMesh
    {
    private:
        const double DEFAULT_STIFFNESS = 10.0;
        const double DEFAULT_REST_LENGTH = 1.0e-3;
        const double DEFAULT_SPEED_LIMIT = 100.0;

        SpringList springList;
        BallList currBallList;
        BallList midBallList;
        BallList nextBallList;
        PhysicsVectorList forceList;                // holds calculated net force on each ball
        PhysicsVector gravity;
        double stiffness = DEFAULT_STIFFNESS;       // the linear spring constant [N/m]
        double restLength = DEFAULT_REST_LENGTH;    // spring length [m] that results in zero force
        double speedLimit = DEFAULT_SPEED_LIMIT;

    public:
        void Reset();
        double GetStiffness() const { return stiffness; }
        void SetStiffness(double _stiffness);
        double GetRestLength() const { return restLength; }
        void SetRestLength(double _restLength);
        double GetSpeedLimit() const { return speedLimit; }
        void SetSpeedLimit(double _speedLimit) { speedLimit = _speedLimit; }
        PhysicsVector GetGravity() const { return gravity; }
        void SetGravity(PhysicsVector _gravity) { gravity = _gravity; }
        int Add(Ball);      // returns ball index, for linking with springs
        bool Add(Spring);   // returns false if either ball index is bad, true if spring added
        const SpringList& GetSprings() const { return springList; }
        const BallList& GetBalls() const { return currBallList; }
        void Update(double dt, double halflife);
        int NumBalls() const { return static_cast<int>(currBallList.size()); }
        int NumSprings() const { return static_cast<int>(springList.size()); }
        Ball& GetBallAt(int index) { return currBallList.at(index); }
        Spring& GetSpringAt(int index) { return springList.at(index); }

    private:
        void CalcForces(
            BallList& blist,
            PhysicsVectorList& forceList);

        static void Dampen(BallList& blist, double dt, double halflife);
        static void Copy(const BallList& source, BallList& target);

        static void Extrapolate(
            double dt,
            double speedLimit,
            const PhysicsVectorList& forceList,
            const BallList& sourceList,
            BallList& targetList
        );
    };


    class HighPassFilter
    {
    private:
        bool first;
        double xprev;
        double yprev;
        double fc;

    public:
        HighPassFilter(double cutoffFrequencyHz);
        void Reset() { first = true; }
        void SetCutoffFrequency(double cutoffFrequencyHz) { fc = cutoffFrequencyHz; }
        double Update(double x, double sampleRateHz);
    };

    void MeshFromJson(PhysicsMesh &mesh, json_t* root);
    json_t *JsonFromMesh(const PhysicsMesh &mesh);
}
