//****  GENERATED CODE  ****  DO NOT EDIT  *****

#include "elastika_mesh.hpp"

namespace Sapphire
{
    ElastikaMesh::ElastikaMesh()
    {
        Add(Ball(1e-06, 0.001, 0, 0));
        Add(Ball(1e-06, 0.0005, 0.0008660255, 0));
        Add(Ball(1e-06, -0.0005, 0.0008660255, 0));
        Add(Ball(1e-06, -0.001, 0, 0));
        Add(Ball(1e-06, -0.0005, -0.0008660255, 0));
        Add(Ball(1e-06, 0.0005, -0.0008660255, 0));
        Add(Ball(1e-06, 0.0025, 0.0008660255, 0));
        Add(Ball(1e-06, 0.002, 0.001732051, 0));
        Add(Ball(1e-06, 0.001, 0.001732051, 0));
        Add(Ball(1e-06, 0.002, 0, 0));
        Add(Ball(1e-06, 0.004, 0.001732051, 0));
        Add(Ball(1e-06, 0.0035, 0.002598076, 0));
        Add(Ball(1e-06, 0.0025, 0.002598076, 0));
        Add(Ball(1e-06, 0.0035, 0.0008660255, 0));
        Add(Ball(1e-06, 0.001, -0.001732051, 0));
        Add(Ball(1e-06, -0.001, -0.001732051, 0));
        Add(Ball(1e-06, -0.0005, -0.002598076, 0));
        Add(Ball(1e-06, 0.0005, -0.002598076, 0));
        Add(Ball(1e-06, 0.0025, -0.0008660255, 0));
        Add(Ball(1e-06, 0.002, -0.001732051, 0));
        Add(Ball(1e-06, 0.004, 0, 0));
        Add(Ball(1e-06, 0.0035, -0.0008660255, 0));
        Add(Ball(-1, -0.002, 0, 0));
        Add(Ball(-1, -0.001, 0.001732051, 0));
        Add(Ball(-1, -0.002, -0.001732051, 0));
        Add(Ball(-1, 0.0005, 0.002598076, 0));
        Add(Ball(-1, -0.001, -0.003464102, 0));
        Add(Ball(-1, 0.002, 0.003464102, 0));
        Add(Ball(-1, 0.001, -0.003464102, 0));
        Add(Ball(-1, 0.004, 0.003464102, 0));
        Add(Ball(-1, 0.0025, -0.002598076, 0));
        Add(Ball(-1, 0.005, 0.001732051, 0));
        Add(Ball(-1, 0.004, -0.001732051, 0));
        Add(Ball(-1, 0.005, 0, 0));

        Add(Spring(3, 2));
        Add(Spring(3, 22));
        Add(Spring(3, 4));
        Add(Spring(2, 1));
        Add(Spring(2, 23));
        Add(Spring(15, 4));
        Add(Spring(15, 24));
        Add(Spring(15, 16));
        Add(Spring(4, 5));
        Add(Spring(1, 8));
        Add(Spring(1, 0));
        Add(Spring(8, 7));
        Add(Spring(8, 25));
        Add(Spring(16, 17));
        Add(Spring(16, 26));
        Add(Spring(5, 0));
        Add(Spring(5, 14));
        Add(Spring(0, 9));
        Add(Spring(7, 12));
        Add(Spring(7, 6));
        Add(Spring(12, 11));
        Add(Spring(12, 27));
        Add(Spring(17, 14));
        Add(Spring(17, 28));
        Add(Spring(14, 19));
        Add(Spring(9, 6));
        Add(Spring(9, 18));
        Add(Spring(6, 13));
        Add(Spring(11, 29));
        Add(Spring(11, 10));
        Add(Spring(19, 18));
        Add(Spring(19, 30));
        Add(Spring(18, 21));
        Add(Spring(13, 10));
        Add(Spring(13, 20));
        Add(Spring(10, 31));
        Add(Spring(21, 20));
        Add(Spring(21, 32));
        Add(Spring(20, 33));
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
