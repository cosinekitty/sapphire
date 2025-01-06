#pragma once
#include "sapphire_engine.hpp"

namespace Sapphire
{
    const float BallPositionFactor = 6000;      // how much to scale voltages based on ball displacements [V/m]

    struct Ball
    {
        PhysicsVector pos;  // position [m]
        PhysicsVector vel;  // velocity [m/s]
        float mass;         // mass [kg], or negative to indicate infinite mass (an "anchor")

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

    using BallList = std::vector<Ball>;

    const float MESH_DEFAULT_STIFFNESS = 10.0;
    const float MESH_DEFAULT_REST_LENGTH = 1.0e-3;
    const float MESH_DEFAULT_SPEED_LIMIT = 2.0;

    class PhysicsMesh
    {
    protected:
        std::vector<PhysicsVector> originalPositions;
        BallList currBallList;
        BallList nextBallList;
        PhysicsVectorList forceList;                // holds calculated net force on each ball
        PhysicsVector gravity;
        PhysicsVector magnet;
        float stiffness  = MESH_DEFAULT_STIFFNESS;     // the linear spring constant [N/m]
        float restLength = MESH_DEFAULT_REST_LENGTH;   // spring length [m] that results in zero force
        float speedLimit = MESH_DEFAULT_SPEED_LIMIT;

        PhysicsMesh(std::size_t totalBallCount, std::size_t mobileBallCount)
        {
            originalPositions.reserve(totalBallCount);
            currBallList.reserve(totalBallCount);
            nextBallList.reserve(totalBallCount);
            forceList.resize(mobileBallCount, PhysicsVector::zero());
        }

    public:
        PhysicsMesh() {}

        void Quiet();    // put all balls back to their original locations and zero their velocities
        float GetStiffness() const { return stiffness; }
        void SetStiffness(float _stiffness) { stiffness = std::max(0.0f, _stiffness); }
        float GetRestLength() const { return restLength; }
        void SetRestLength(float _restLength) { restLength = std::max(0.0f, _restLength); }
        float GetSpeedLimit() const { return speedLimit; }
        void SetSpeedLimit(float _speedLimit) { speedLimit = _speedLimit; }
        void SetMagneticField(PhysicsVector _magnet) { magnet = _magnet; }
        PhysicsVector GetGravity() const { return gravity; }
        void SetGravity(PhysicsVector _gravity) { gravity = _gravity; }
        int AddBall(Ball);      // returns ball index, for linking with springs
        int AddBall(float mass, float rx, float ry, float rz) { return AddBall(Ball(mass, rx, ry, rz)); }
        BallList& GetBalls() { return currBallList; }
        int NumBalls() const { return static_cast<int>(currBallList.size()); }
        const Ball& GetBallAt(int index) const { return currBallList.at(index); }
        bool IsAnchor(int ballIndex) const { return GetBallAt(ballIndex).IsAnchor(); }
        bool IsMobile(int ballIndex) const { return GetBallAt(ballIndex).IsMobile(); }
        PhysicsVector GetBallOrigin(int index) const { return originalPositions.at(index); }
        PhysicsVector GetBallDisplacement(int index) const { return currBallList.at(index).pos - originalPositions.at(index); }

        void SetBallPosition(int index, const PhysicsVector& pos)
        {
            currBallList.at(index).pos = pos;
            nextBallList.at(index).pos = pos;
        }

        void SetBallMass(int index, float mass)
        {
            currBallList.at(index).mass = mass;
            nextBallList.at(index).mass = mass;
        }
    };


    struct MeshAudioParameters
    {
        int leftInputBallIndex      = -1;
        int rightInputBallIndex     = -1;
        int leftOutputBallIndex     = -1;
        int rightOutputBallIndex    = -1;
        int leftVarMassBallIndex    = -1;
        int rightVarMassBallIndex   = -1;
        PhysicsVector leftInputDir1;
        PhysicsVector leftInputDir2;
        PhysicsVector rightInputDir1;
        PhysicsVector rightInputDir2;
        PhysicsVector leftOutputDir1;
        PhysicsVector leftOutputDir2;
        PhysicsVector rightOutputDir1;
        PhysicsVector rightOutputDir2;
    };
}
