#include <vector>
#include <assert.h>
#include "mesh.hpp"

namespace Sapphire
{
    class ImageMap
    {
    private:
        int radius;
        std::vector<int> table;

        int TableIndex(int i, int j) const
        {
            int x = (radius+1) + i;
            int y = (radius+1) + j;
            int width = 3 + (2 * radius);
            return x + (width * y);
        }

        static size_t TableSize(int radius)
        {
            size_t width = 3 + (2 * radius);
            return width * width;
        }

    public:
        ImageMap(int _radius)
            : radius(_radius)
            , table(TableSize(_radius), -1)
            {}

        void Store(int i, int j, int index)
        {
            int &t = table.at(TableIndex(i, j));
            assert(t < 0);      // otherwise we are trying to place two balls in the same table location.
            t = index;
        }

        int Get(int i, int j) const
        {
            if (i < -radius || i > +radius || j < -radius || j > +radius)
                return -1;

            return table.at(TableIndex(i, j));
        }
    };


    int AnchorIndex(Sapphire::PhysicsMesh& mesh, ImageMap& map, int i, int j, double spacing)
    {
        int index = map.Get(i, j);
        if (index < 0)
        {
            index = mesh.Add(Sapphire::Ball::Anchor(i*spacing, j*spacing, 0.0));
            map.Store(i, j, index);
        }
        return index;
    }


    MeshAudioParameters CreateRoundDrum(Sapphire::PhysicsMesh& mesh)
    {
        using namespace Sapphire;
        const int radius = 3;
        const double mass = 1.0e-6;
        const double spacing = MESH_DEFAULT_REST_LENGTH;

        mesh.Reset();

        ImageMap map(radius);

        // Create mobile balls for every point inside or on the edge of the circle.
        for (int i = -radius; i <= +radius; ++i)
        {
            for (int j = -radius; j <= +radius; ++j)
            {
                if (i*i + j*j <= radius*radius)
                {
                    int index = mesh.Add(Ball(mass, spacing*i, spacing*j, 0.0));
                    map.Store(i, j, index);
                }
            }
        }

        // Create anchor points as needed outside the circle.
        for (int i = -radius; i <= +radius; ++i)
        {
            for (int j = -radius; j <= +radius; ++j)
            {
                if (i*i + j*j <= radius*radius)
                {
                    int indexHere  = map.Get(i, j);

                    int indexAbove = AnchorIndex(mesh, map, i, j+1, spacing);
                    mesh.Add(Spring(indexHere, indexAbove));

                    int indexLeft  = AnchorIndex(mesh, map, i+1, j, spacing);
                    mesh.Add(Spring(indexHere, indexLeft));

                    if (i*i + (j-1)*(j-1) > radius*radius)
                    {
                        int indexBelow = AnchorIndex(mesh, map, i, j-1, spacing);
                        mesh.Add(Spring(indexHere, indexBelow));
                    }

                    if ((i-1)*(i-1) + j*j > radius*radius)
                    {
                        int indexRight = AnchorIndex(mesh, map, i-1, j, spacing);
                        mesh.Add(Spring(indexHere, indexRight));
                    }
                }
            }
        }

        int leftInputBallIndex   = map.Get(-2, -1);
        int rightInputBallIndex  = map.Get(-2, +1);
        int leftOutputBallIndex  = map.Get(+1, -2);
        int rightOutputBallIndex = map.Get(+1, +2);

        return MeshAudioParameters(
            leftInputBallIndex,
            rightInputBallIndex,
            leftOutputBallIndex,
            rightOutputBallIndex
        );
    }
}
