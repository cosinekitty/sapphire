#pragma once
#include "mesh_physics.hpp"

namespace Sapphire
{
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

    MeshAudioParameters CreateHex(PhysicsMesh& mesh);
}
