#pragma once
#include "mesh_physics.hpp"

namespace Sapphire
{
    struct Spring
    {
        int ballIndex1;         // 0-based index into Mesh::ballList
        int ballIndex2;         // 0-based index into Mesh::ballList

        Spring(int _ballIndex1, int _ballIndex2)
            : ballIndex1(_ballIndex1)
            , ballIndex2(_ballIndex2)
            {}
    };

    using SpringList = std::vector<Spring>;

    class PhysicsMeshGen : public PhysicsMesh   // Includes explicit springs. Used by code generator only.
    {
    protected:
        SpringList springList;

    public:
        const SpringList& GetSprings() const { return springList; }
        SpringList& GetSprings() { return springList; }
        int NumSprings() const { return static_cast<int>(springList.size()); }
        Spring& GetSpringAt(int index) { return springList.at(index); }

        bool AddSpring(Spring spring)
        {
            const int nballs = static_cast<int>(currBallList.size());

            if (spring.ballIndex1 < 0 || spring.ballIndex1 >= nballs)
                return false;

            if (spring.ballIndex2 < 0 || spring.ballIndex2 >= nballs)
                return false;

            springList.push_back(spring);
            return true;
        }
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

    MeshAudioParameters CreateHex(PhysicsMeshGen& mesh);
}
