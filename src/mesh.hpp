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
    const float MESH_DEFAULT_SPEED_LIMIT = 100.0;

    class PhysicsMesh
    {
    private:
        SpringList springList;
        BallList currBallList;
        BallList nextBallList;
        PhysicsVectorList forceList;                // holds calculated net force on each ball
        PhysicsVector gravity;
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
        PhysicsVector GetGravity() const { return gravity; }
        void SetGravity(PhysicsVector _gravity) { gravity = _gravity; }
        int Add(Ball);      // returns ball index, for linking with springs
        bool Add(Spring);   // returns false if either ball index is bad, true if spring added
        const SpringList& GetSprings() const { return springList; }
        const BallList& GetBalls() const { return currBallList; }
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


    class HighPassFilter
    {
    private:
        bool first;
        float xprev;
        float yprev;
        float fc;

    public:
        HighPassFilter(float cutoffFrequencyHz);
        void Reset() { first = true; }
        void SetCutoffFrequency(float cutoffFrequencyHz) { fc = cutoffFrequencyHz; }
        float Update(float x, float sampleRateHz);
    };

    struct MeshAudioParameters
    {
        int leftInputBallIndex;
        int rightInputBallIndex;
        int leftOutputBallIndex;
        int rightOutputBallIndex;
        PhysicsVector leftStimulus;
        PhysicsVector rightStimulus;
        PhysicsVector leftResponse;
        PhysicsVector rightResponse;

        MeshAudioParameters(
            int _leftInputBallIndex,
            int _rightInputBallIndex,
            int _leftOutputBallIndex,
            int _rightOutputBallIndex,
            PhysicsVector _leftStimulus,
            PhysicsVector _rightStimulus,
            PhysicsVector _leftResponse,
            PhysicsVector _rightResponse
        )
            : leftInputBallIndex(_leftInputBallIndex)
            , rightInputBallIndex(_rightInputBallIndex)
            , leftOutputBallIndex(_leftOutputBallIndex)
            , rightOutputBallIndex(_rightOutputBallIndex)
            , leftStimulus(_leftStimulus)
            , rightStimulus(_rightStimulus)
            , leftResponse(_leftResponse)
            , rightResponse(_rightResponse)
            {}
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
