#pragma once
#include "sapphire_engine.hpp"

namespace Sapphire
{
    const float BallPositionFactor = 6000;      // how much to scale voltages based on ball displacements [V/m]

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

    using SpringList = std::vector<Spring>;
    using BallList = std::vector<Ball>;

    const float MESH_DEFAULT_STIFFNESS = 10.0;
    const float MESH_DEFAULT_REST_LENGTH = 1.0e-3;
    const float MESH_DEFAULT_SPEED_LIMIT = 2.0;

    class PhysicsMesh
    {
    protected:
        SpringList springList;
        std::vector<PhysicsVector> originalPositions;
        BallList currBallList;
        BallList nextBallList;
        PhysicsVectorList forceList;                // holds calculated net force on each ball
        PhysicsVector gravity;
        PhysicsVector magnet;
        float stiffness  = MESH_DEFAULT_STIFFNESS;     // the linear spring constant [N/m]
        float restLength = MESH_DEFAULT_REST_LENGTH;   // spring length [m] that results in zero force
        float speedLimit = MESH_DEFAULT_SPEED_LIMIT;

        virtual void CalcForces(
            const BallList& blist,
            PhysicsVectorList& forceList);

    public:
        void Clear();   // empty out the mesh and start over
        void Quiet();    // put all balls back to their original locations and zero their velocities
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
        const Ball& GetBallAt(int index) const { return currBallList.at(index); }
        bool IsAnchor(int ballIndex) const { return GetBallAt(ballIndex).IsAnchor(); }
        bool IsMobile(int ballIndex) const { return GetBallAt(ballIndex).IsMobile(); }
        PhysicsVector GetBallOrigin(int index) const { return originalPositions.at(index); }
        PhysicsVector GetBallDisplacement(int index) const { return currBallList.at(index).pos - originalPositions.at(index); }
        Spring& GetSpringAt(int index) { return springList.at(index); }

    private:
        virtual void Dampen(BallList& blist, float dt, float halflife);

        static void Extrapolate(
            float dt,
            float speedLimit,
            const PhysicsVectorList& forceList,
            const BallList& sourceList,
            BallList& targetList
        );
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
