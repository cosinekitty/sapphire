#include <vector>
#include <assert.h>
#include "mesh.hpp"

namespace Sapphire
{
    MeshAudioParameters CreateString(Sapphire::PhysicsMesh& mesh)
    {
        using namespace Sapphire;
        const int nMobileBalls = 30;
        const double mass = 1.0e-6;
        const double spacing = MESH_DEFAULT_REST_LENGTH;

        mesh.Reset();

        mesh.Add(Ball::Anchor(0.0, 0.0, 0.0));
        for (int b = 1; b <= nMobileBalls; ++b)
            mesh.Add(Ball(mass, b*spacing, 0.0, 0.0));
        mesh.Add(Ball::Anchor((nMobileBalls+1)*spacing, 0.0, 0.0));

        for (int s = 0; s <= nMobileBalls; ++s)
            mesh.Add(Spring(s, s+1));

        return MeshAudioParameters(
            3,
            6,
            nMobileBalls - 3,
            nMobileBalls - 6
        );
    }
}
