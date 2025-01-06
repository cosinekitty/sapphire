#pragma once
#include "mesh_physics.hpp"

namespace Sapphire
{
    // This class custom-optimizes the force calculator for the particular
    // mesh layout that Elastika uses.

    class ElastikaMesh : public PhysicsMesh
    {
    public:
        ElastikaMesh();
        static MeshAudioParameters getAudioParameters();

        void Update(float dt, float halflife)
        {
            Dampen(dt, halflife);
            CalcForces(currBallList);
            Extrapolate(dt/2);
            CalcForces(nextBallList);
            Extrapolate(dt);
            std::swap(nextBallList, currBallList);
        }

    private:
        void Dampen(float dt, float halflife);
        void CalcForces(const BallList& blist);
        void Extrapolate(float dt);
    };
}
