//****  GENERATED CODE  ****  DO NOT EDIT  *****

#include "elastika_mesh.hpp"

namespace Sapphire
{
    ElastikaMesh::ElastikaMesh()
        : PhysicsMesh(34, 22)
    {
        AddBall(  1e-06,   0.001,              0, 0);   //  0
        AddBall(  1e-06,  0.0005,   0.0008660255, 0);   //  1
        AddBall(  1e-06, -0.0005,   0.0008660255, 0);   //  2
        AddBall(  1e-06,  -0.001,              0, 0);   //  3
        AddBall(  1e-06, -0.0005,  -0.0008660255, 0);   //  4
        AddBall(  1e-06,  0.0005,  -0.0008660255, 0);   //  5
        AddBall(  1e-06,  0.0025,   0.0008660255, 0);   //  6
        AddBall(  1e-06,   0.002,    0.001732051, 0);   //  7
        AddBall(  1e-06,   0.001,    0.001732051, 0);   //  8
        AddBall(  1e-06,   0.002,              0, 0);   //  9
        AddBall(  1e-06,   0.004,    0.001732051, 0);   // 10
        AddBall(  1e-06,  0.0035,    0.002598076, 0);   // 11
        AddBall(  1e-06,  0.0025,    0.002598076, 0);   // 12
        AddBall(  1e-06,  0.0035,   0.0008660255, 0);   // 13
        AddBall(  1e-06,   0.001,   -0.001732051, 0);   // 14
        AddBall(  1e-06,  -0.001,   -0.001732051, 0);   // 15
        AddBall(  1e-06, -0.0005,   -0.002598076, 0);   // 16
        AddBall(  1e-06,  0.0005,   -0.002598076, 0);   // 17
        AddBall(  1e-06,  0.0025,  -0.0008660255, 0);   // 18
        AddBall(  1e-06,   0.002,   -0.001732051, 0);   // 19
        AddBall(  1e-06,   0.004,              0, 0);   // 20
        AddBall(  1e-06,  0.0035,  -0.0008660255, 0);   // 21
        AddBall(     -1,  -0.002,              0, 0);   // 22
        AddBall(     -1,  -0.001,    0.001732051, 0);   // 23
        AddBall(     -1,  -0.002,   -0.001732051, 0);   // 24
        AddBall(     -1,  0.0005,    0.002598076, 0);   // 25
        AddBall(     -1,  -0.001,   -0.003464102, 0);   // 26
        AddBall(     -1,   0.002,    0.003464102, 0);   // 27
        AddBall(     -1,   0.001,   -0.003464102, 0);   // 28
        AddBall(     -1,   0.004,    0.003464102, 0);   // 29
        AddBall(     -1,  0.0025,   -0.002598076, 0);   // 30
        AddBall(     -1,   0.005,    0.001732051, 0);   // 31
        AddBall(     -1,   0.004,   -0.001732051, 0);   // 32
        AddBall(     -1,   0.005,              0, 0);   // 33
    }

    void ElastikaMesh::Dampen(float dt, float halflife)
    {
        const float damp = OneHalfToPower(dt/halflife);
        currBallList[ 0].vel *= damp;
        currBallList[ 1].vel *= damp;
        currBallList[ 2].vel *= damp;
        currBallList[ 3].vel *= damp;
        currBallList[ 4].vel *= damp;
        currBallList[ 5].vel *= damp;
        currBallList[ 6].vel *= damp;
        currBallList[ 7].vel *= damp;
        currBallList[ 8].vel *= damp;
        currBallList[ 9].vel *= damp;
        currBallList[10].vel *= damp;
        currBallList[11].vel *= damp;
        currBallList[12].vel *= damp;
        currBallList[13].vel *= damp;
        currBallList[14].vel *= damp;
        currBallList[15].vel *= damp;
        currBallList[16].vel *= damp;
        currBallList[17].vel *= damp;
        currBallList[18].vel *= damp;
        currBallList[19].vel *= damp;
        currBallList[20].vel *= damp;
        currBallList[21].vel *= damp;
    }

    void ElastikaMesh::CalcForces(const BallList& blist)
    {
        forceList[3] = Cross(blist[3].vel, magnet);
        forceList[2] = Cross(blist[2].vel, magnet);
        PhysicsVector dr = blist[2].pos - blist[3].pos;
        float dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 3] += force;
            forceList[ 2] -= force;
        }

        dr = blist[22].pos - blist[3].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
            forceList[3] += ((stiffness * (dist - restLength)) / dist) * dr;

        forceList[4] = Cross(blist[4].vel, magnet);
        dr = blist[4].pos - blist[3].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 3] += force;
            forceList[ 4] -= force;
        }

        forceList[1] = Cross(blist[1].vel, magnet);
        dr = blist[1].pos - blist[2].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 2] += force;
            forceList[ 1] -= force;
        }

        dr = blist[23].pos - blist[2].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
            forceList[2] += ((stiffness * (dist - restLength)) / dist) * dr;

        forceList[15] = Cross(blist[15].vel, magnet);
        dr = blist[4].pos - blist[15].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[15] += force;
            forceList[ 4] -= force;
        }

        dr = blist[24].pos - blist[15].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
            forceList[15] += ((stiffness * (dist - restLength)) / dist) * dr;

        forceList[16] = Cross(blist[16].vel, magnet);
        dr = blist[16].pos - blist[15].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[15] += force;
            forceList[16] -= force;
        }

        forceList[5] = Cross(blist[5].vel, magnet);
        dr = blist[5].pos - blist[4].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 4] += force;
            forceList[ 5] -= force;
        }

        forceList[8] = Cross(blist[8].vel, magnet);
        dr = blist[8].pos - blist[1].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 1] += force;
            forceList[ 8] -= force;
        }

        forceList[0] = Cross(blist[0].vel, magnet);
        dr = blist[0].pos - blist[1].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 1] += force;
            forceList[ 0] -= force;
        }

        forceList[7] = Cross(blist[7].vel, magnet);
        dr = blist[7].pos - blist[8].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 8] += force;
            forceList[ 7] -= force;
        }

        dr = blist[25].pos - blist[8].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
            forceList[8] += ((stiffness * (dist - restLength)) / dist) * dr;

        forceList[17] = Cross(blist[17].vel, magnet);
        dr = blist[17].pos - blist[16].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[16] += force;
            forceList[17] -= force;
        }

        dr = blist[26].pos - blist[16].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
            forceList[16] += ((stiffness * (dist - restLength)) / dist) * dr;

        dr = blist[0].pos - blist[5].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 5] += force;
            forceList[ 0] -= force;
        }

        forceList[14] = Cross(blist[14].vel, magnet);
        dr = blist[14].pos - blist[5].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 5] += force;
            forceList[14] -= force;
        }

        forceList[9] = Cross(blist[9].vel, magnet);
        dr = blist[9].pos - blist[0].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 0] += force;
            forceList[ 9] -= force;
        }

        forceList[12] = Cross(blist[12].vel, magnet);
        dr = blist[12].pos - blist[7].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 7] += force;
            forceList[12] -= force;
        }

        forceList[6] = Cross(blist[6].vel, magnet);
        dr = blist[6].pos - blist[7].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 7] += force;
            forceList[ 6] -= force;
        }

        forceList[11] = Cross(blist[11].vel, magnet);
        dr = blist[11].pos - blist[12].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[12] += force;
            forceList[11] -= force;
        }

        dr = blist[27].pos - blist[12].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
            forceList[12] += ((stiffness * (dist - restLength)) / dist) * dr;

        dr = blist[14].pos - blist[17].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[17] += force;
            forceList[14] -= force;
        }

        dr = blist[28].pos - blist[17].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
            forceList[17] += ((stiffness * (dist - restLength)) / dist) * dr;

        forceList[19] = Cross(blist[19].vel, magnet);
        dr = blist[19].pos - blist[14].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[14] += force;
            forceList[19] -= force;
        }

        dr = blist[6].pos - blist[9].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 9] += force;
            forceList[ 6] -= force;
        }

        forceList[18] = Cross(blist[18].vel, magnet);
        dr = blist[18].pos - blist[9].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 9] += force;
            forceList[18] -= force;
        }

        forceList[13] = Cross(blist[13].vel, magnet);
        dr = blist[13].pos - blist[6].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[ 6] += force;
            forceList[13] -= force;
        }

        dr = blist[29].pos - blist[11].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
            forceList[11] += ((stiffness * (dist - restLength)) / dist) * dr;

        forceList[10] = Cross(blist[10].vel, magnet);
        dr = blist[10].pos - blist[11].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[11] += force;
            forceList[10] -= force;
        }

        dr = blist[18].pos - blist[19].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[19] += force;
            forceList[18] -= force;
        }

        dr = blist[30].pos - blist[19].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
            forceList[19] += ((stiffness * (dist - restLength)) / dist) * dr;

        forceList[21] = Cross(blist[21].vel, magnet);
        dr = blist[21].pos - blist[18].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[18] += force;
            forceList[21] -= force;
        }

        dr = blist[10].pos - blist[13].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[13] += force;
            forceList[10] -= force;
        }

        forceList[20] = Cross(blist[20].vel, magnet);
        dr = blist[20].pos - blist[13].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[13] += force;
            forceList[20] -= force;
        }

        dr = blist[31].pos - blist[10].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
            forceList[10] += ((stiffness * (dist - restLength)) / dist) * dr;

        dr = blist[20].pos - blist[21].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
        {
            PhysicsVector force = ((stiffness * (dist - restLength)) / dist) * dr;
            forceList[21] += force;
            forceList[20] -= force;
        }

        dr = blist[32].pos - blist[21].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
            forceList[21] += ((stiffness * (dist - restLength)) / dist) * dr;

        dr = blist[33].pos - blist[20].pos;
        dist = Magnitude(dr);
        if (dist >= 1.0e-9f)
            forceList[20] += ((stiffness * (dist - restLength)) / dist) * dr;
    }

    void ElastikaMesh::Extrapolate(float dt)
    {
        const float speedLimitSquared = speedLimit * speedLimit;
        const Ball* curr = currBallList.data();
        Ball* next = nextBallList.data();
        float speedSquared;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[0]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[1]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[2]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[3]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[4]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[5]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[6]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[7]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[8]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[9]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[10]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[11]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[12]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[13]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[14]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[15]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[16]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[17]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[18]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[19]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[20]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
        ++curr;
        ++next;

        next->vel = curr->vel + ((dt / curr->mass) * forceList[21]);
        speedSquared = Quadrature(next->vel);
        if (speedSquared > speedLimitSquared)
            next->vel *= speedLimit / std::sqrt(speedSquared);
        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));
    }

    MeshAudioParameters ElastikaMesh::getAudioParameters()
    {
        MeshAudioParameters mp;
        mp.leftInputBallIndex = 23;
        mp.rightInputBallIndex = 32;
        mp.leftOutputBallIndex = 12;
        mp.rightOutputBallIndex = 17;
        mp.leftVarMassBallIndex = 6;
        mp.rightVarMassBallIndex = 5;
        mp.leftInputDir1 = PhysicsVector{0, 0, 0.0001, 0};
        mp.leftInputDir2 = PhysicsVector{7e-05, -7e-05, 0, 0};
        mp.rightInputDir1 = PhysicsVector{0, 0, 0.0001, 0};
        mp.rightInputDir2 = PhysicsVector{-7e-05, 7e-05, 0, 0};
        mp.leftOutputDir1 = PhysicsVector{0, 0, 6000, 0};
        mp.leftOutputDir2 = PhysicsVector{0, -6000, 0, 0};
        mp.rightOutputDir1 = PhysicsVector{0, 0, 6000, 0};
        mp.rightOutputDir2 = PhysicsVector{0, 6000, 0, 0};
        return mp;
    }
}
