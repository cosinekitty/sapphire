#pragma once

// Sapphire mesh physics engine, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire


#include <exception>
#include <stdexcept>
#include <string>
#include <vector>
#include <jansson.h>
#include <rack.hpp>

namespace Sapphire
{
    using PhysicsVector = rack::simd::float_4;

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
        return sqrt(Dot(a, a));
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
        float mass;            // the ball's [kg]

        Ball(float _mass, float _x, float _y, float _z)
            : pos(_x, _y, _z, 0.0f)
            , vel()
            , mass(_mass)
            {}

        Ball(float _mass, PhysicsVector _pos, PhysicsVector _vel)
            : pos(_pos)
            , vel(_vel)
            , mass(_mass)
            {}

        static Ball Anchor(float _x, float _y, float _z)
        {
            return Ball(-1.0, _x, _y, _z);
        }

        static Ball Anchor(PhysicsVector pos)
        {
            return Ball(-1.0, pos.s[0], pos.s[1], pos.s[2]);
        }

        bool IsAnchor() const { return mass <= 0.0; }
        bool IsMobile() const { return mass > 0.0; }
    };

    typedef std::vector<Spring> SpringList;
    typedef std::vector<Ball> BallList;
    typedef std::vector<PhysicsVector> PhysicsVectorList;
    typedef std::vector<float> PhysicsScalarList;

    const float MESH_DEFAULT_STIFFNESS = 10.0;
    const float MESH_DEFAULT_REST_LENGTH = 1.0e-3;
    const float MESH_DEFAULT_SPEED_LIMIT = 2.0;

    class PhysicsMesh
    {
    private:
        SpringList springList;
        BallList currBallList;
        BallList nextBallList;
        PhysicsVectorList forceList;                // holds calculated net force on each ball
        PhysicsVector gravity;
        PhysicsVector magnet;
        float stiffness  = MESH_DEFAULT_STIFFNESS;     // the linear spring constant [N/m]
        float restLength = MESH_DEFAULT_REST_LENGTH;   // spring length [m] that results in zero force
        float speedLimit = MESH_DEFAULT_SPEED_LIMIT;

    public:
        void Reset();
        float GetStiffness() const { return stiffness; }
        void SetStiffness(float _stiffness);
        float GetRestLength() const { return restLength; }
        void SetRestLength(float _restLength);
        float GetSpeedLimit() const { return speedLimit; }
        void SetSpeedLimit(float _speedLimit) { speedLimit = _speedLimit; }
        void SetMagneticField(PhysicsVector _magnet) { magnet = _magnet; }
        PhysicsVector GetGravity() const { return gravity; }
        void SetGravity(PhysicsVector _gravity) { gravity = _gravity; }
        int Add(Ball);      // returns ball index, for linking with springs
        bool Add(Spring);   // returns false if either ball index is bad, true if spring added
        const SpringList& GetSprings() const { return springList; }
        BallList& GetBalls() { return currBallList; }
        void Update(float dt, float halflife);
        int NumBalls() const { return static_cast<int>(currBallList.size()); }
        int NumSprings() const { return static_cast<int>(springList.size()); }
        Ball& GetBallAt(int index) { return currBallList.at(index); }
        Spring& GetSpringAt(int index) { return springList.at(index); }

    private:
        void CalcForces(
            BallList& blist,
            PhysicsVectorList& forceList);

        static void Dampen(BallList& blist, float dt, float halflife);
        static void Copy(const BallList& source, BallList& target);

        static void Extrapolate(
            float dt,
            float speedLimit,
            const PhysicsVectorList& forceList,
            const BallList& sourceList,
            BallList& targetList
        );
    };


    class LoHiPassFilter
    {
    private:
        bool  first;
        float xprev;
        float yprev;
        float fc;

    public:
        LoHiPassFilter()
            : first(true)
            , xprev(0.0)
            , yprev(0.0)
            , fc(20.0)
            {}

        void Reset() { first = true; }
        void SetCutoffFrequency(float cutoffFrequencyHz) { fc = cutoffFrequencyHz; }
        void Update(float x, float sampleRateHz);
        float HiPass() const { return xprev - yprev; }
        float LoPass() const { return yprev; };
    };


    template <int LAYERS>
    class StagedFilter
    {
    private:
        LoHiPassFilter stage[LAYERS];

    public:
        void Reset()
        {
            for (int i = 0; i < LAYERS; ++i)
                stage[i].Reset();
        }

        void SetCutoffFrequency(float cutoffFrequencyHz)
        {
            for (int i = 0; i < LAYERS; ++i)
                stage[i].SetCutoffFrequency(cutoffFrequencyHz);
        }

        float UpdateLoPass(float x, float sampleRateHz)
        {
            float y = x;
            for (int i=0; i < LAYERS; ++i)
            {
                stage[i].Update(y, sampleRateHz);
                y = stage[i].LoPass();
            }
            return y;
        }

        float UpdateHiPass(float x, float sampleRateHz)
        {
            float y = x;
            for (int i=0; i < LAYERS; ++i)
            {
                stage[i].Update(y, sampleRateHz);
                y = stage[i].HiPass();
            }
            return y;
        }
    };


    struct MeshAudioParameters
    {
        int leftInputBallIndex;
        int rightInputBallIndex;
        int leftOutputBallIndex;
        int rightOutputBallIndex;
        PhysicsVector leftStimulus1;
        PhysicsVector leftStimulus2;
        PhysicsVector rightStimulus1;
        PhysicsVector rightStimulus2;
        PhysicsVector leftResponse1;
        PhysicsVector leftResponse2;
        PhysicsVector rightResponse1;
        PhysicsVector rightResponse2;
    };

    // GridMap is a read-write 2D array whose indices can be positive or negative.
    template <typename TElement>
    class GridMap
    {
    private:
        const int umin;      // the minimum value of a horizontal index
        const int umax;      // the maximum value of a horizontal index
        const int vmin;      // the minimum value of a vertical index
        const int vmax;      // the maximum value of a vertical index
        std::vector<TElement> array;

    public:
        GridMap(int _umin, int _umax, int _vmin, int _vmax, TElement _init)
            : umin(_umin)
            , umax(_umax)
            , vmin(_vmin)
            , vmax(_vmax)
            , array((_umax-_umin+1)*(_vmax-_vmin+1), _init)
            {}

        TElement& at(int u, int v)
        {
            if (u < umin || u > umax)
                throw std::out_of_range("u");

            if (v < vmin || v > vmax)
                throw std::out_of_range("v");

            return array.at((u-umin) + (umax-umin+1)*(v-vmin));
        }
    };

    // Factory functions that make meshes:
    MeshAudioParameters CreateRoundDrum(PhysicsMesh& mesh);
    MeshAudioParameters CreateString(PhysicsMesh& mesh);
    MeshAudioParameters CreateHex(PhysicsMesh& mesh);
}
