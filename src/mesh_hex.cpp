#include <assert.h>
#include <math.h>
#include "mesh.hpp"

namespace Sapphire
{
    const uint8_t SPRINGDIR_E  = (1 << 0);
    const uint8_t SPRINGDIR_N  = (1 << 1);
    const uint8_t SPRINGDIR_NW = (1 << 2);
    const uint8_t SPRINGDIR_W  = (1 << 3);
    const uint8_t SPRINGDIR_S  = (1 << 4);
    const uint8_t SPRINGDIR_SE = (1 << 5);

    uint8_t OppositeDir(uint8_t dir)
    {
        switch (dir)
        {
        case SPRINGDIR_E:  return SPRINGDIR_W;
        case SPRINGDIR_N:  return SPRINGDIR_S;
        case SPRINGDIR_NW: return SPRINGDIR_SE;
        case SPRINGDIR_W:  return SPRINGDIR_E;
        case SPRINGDIR_S:  return SPRINGDIR_N;
        case SPRINGDIR_SE: return SPRINGDIR_NW;
        default:
            throw std::out_of_range("dir");
        }
    }

    struct HexGridElement
    {
        short ballIndex;
        uint8_t springsNeededMask;  // a combination of 3 required spring directions (SPRINGDIR_... bits)
        uint8_t springsAddedMask;   // directions we have already added springs for

        HexGridElement()
            : ballIndex(-1)
            , springsNeededMask(0)
            , springsAddedMask(0)
            {}
    };


    class HexBuilder
    {
    private:
        GridMap<HexGridElement> map;
        PhysicsMesh& mesh;
        const float spacing;
        const float mass;
        int u1, u2, v1, v2;     // the bounding box that contains all mobile balls

    public:
        HexBuilder(PhysicsMesh& _mesh, int _dimension, float _spacing, float _mass)
            : map(-_dimension, +_dimension, -_dimension, +_dimension, HexGridElement())
            , mesh(_mesh)
            , spacing(_spacing)
            , mass(_mass)
            , u1(1 + _dimension)
            , u2(-(1 + _dimension))
            , v1(1 + _dimension)
            , v2(-(1 + _dimension))
            {}

        void AddHexagon(int w, int f)
        {
            // The center of each hexagon is expressed in [w,f] coordinates.
            // Convert to the triangular grid [u,v].
            int u = f + w;
            int v = f - 2*w;

            // Create balls as needed all the way around the hexagon's center.
            // Remember which directions we will need to connect springs,
            // but do not add any springs yet.
            AddBall(u+1, v+0, SPRINGDIR_E  | SPRINGDIR_NW | SPRINGDIR_S );
            AddBall(u+0, v+1, SPRINGDIR_N  | SPRINGDIR_W  | SPRINGDIR_SE);
            AddBall(u-1, v+1, SPRINGDIR_E  | SPRINGDIR_NW | SPRINGDIR_S );
            AddBall(u-1, v+0, SPRINGDIR_N  | SPRINGDIR_W  | SPRINGDIR_SE);
            AddBall(u+0, v-1, SPRINGDIR_E  | SPRINGDIR_NW | SPRINGDIR_S );
            AddBall(u+1, v-1, SPRINGDIR_N  | SPRINGDIR_W  | SPRINGDIR_SE);
        }

        void Finalize()     // call after adding all hexagons to add the anchor frame and springs
        {
            AddFrame();
            AddSprings();
        }

        int BallIndex(int w, int f, int du, int dv)
        {
            int u = f + w + du;
            int v = f - 2*w + dv;
            const HexGridElement& h = map.at(u, v);
            if (h.ballIndex < 0)
                throw std::out_of_range("No ball exists at specified grid coordinates.");
            return h.ballIndex;
        }

    private:
        void AddFrame()
        {
            // Search the bounding box for all mobile balls (non-anchors).
            // For each mobile ball, search every direction we know we need a spring.
            // If there is no ball in that required direction, add an anchor.
            for (int u = u1; u <= u2; ++u)
            {
                for (int v = v1; v <= v2; ++v)
                {
                    HexGridElement& h = map.at(u, v);
                    if (h.ballIndex >= 0 && mesh.GetBallAt(h.ballIndex).IsMobile())
                    {
                        AddMissingAnchor(SPRINGDIR_E , h, u+1, v+0);
                        AddMissingAnchor(SPRINGDIR_N , h, u+0, v+1);
                        AddMissingAnchor(SPRINGDIR_NW, h, u-1, v+1);
                        AddMissingAnchor(SPRINGDIR_W , h, u-1, v+0);
                        AddMissingAnchor(SPRINGDIR_S , h, u+0, v-1);
                        AddMissingAnchor(SPRINGDIR_SE, h, u+1, v-1);
                    }
                }
            }
        }

        void AddSprings()
        {
            // Search the bounding box for all mobile balls (non-anchors).
            // For each mobile ball, search every direction we know we need a spring.
            // Add a spring if there is not one there already.
            for (int u = u1; u <= u2; ++u)
            {
                for (int v = v1; v <= v2; ++v)
                {
                    HexGridElement& h = map.at(u, v);
                    if (h.ballIndex >= 0 && mesh.GetBallAt(h.ballIndex).IsMobile())
                    {
                        AddMissingSpring(SPRINGDIR_E , h, u+1, v+0);
                        AddMissingSpring(SPRINGDIR_N , h, u+0, v+1);
                        AddMissingSpring(SPRINGDIR_NW, h, u-1, v+1);
                        AddMissingSpring(SPRINGDIR_W , h, u-1, v+0);
                        AddMissingSpring(SPRINGDIR_S , h, u+0, v-1);
                        AddMissingSpring(SPRINGDIR_SE, h, u+1, v-1);
                    }
                }
            }
        }

        void UpdateBoundingBox(int u, int v)
        {
            // Maintain a rectangle that contains all balls.
            // This will help us scan for all balls more efficiently.
            u1 = std::min(u1, u);
            u2 = std::max(u2, u);
            v1 = std::min(v1, v);
            v2 = std::max(v2, v);
        }

        PhysicsVector Location(int u, int v) const
        {
            // Convert from the u-v triangular grid to the x-y cartesian grid.
            return PhysicsVector(
                spacing * (u + v/2.0),
                spacing * (sqrt(0.75)*v),
                0.0f,
                0.0f
            );
        }

        void AddBall(int u, int v, uint8_t springsNeededMask)
        {
            HexGridElement& h = map.at(u, v);
            if (h.ballIndex < 0)     // Add a new ball only if one hasn't been added at this location already.
            {
                assert(h.springsNeededMask == 0);
                assert(h.springsAddedMask == 0);
                PhysicsVector pos = Location(u, v);
                h.ballIndex = mesh.Add(Ball(mass, pos, PhysicsVector::zero()));
                h.springsNeededMask = springsNeededMask;
                h.springsAddedMask = 0;
                UpdateBoundingBox(u, v);
            }
        }

        void AddMissingAnchor(uint8_t dir, HexGridElement& first, int u2, int v2)
        {
            if (first.springsNeededMask & dir)
            {
                HexGridElement& second = map.at(u2, v2);
                if (second.ballIndex < 0)
                {
                    // There is no ball in the direction where the first ball needs a spring.
                    // Create an anchor as a second ball to satisfy this role.
                    PhysicsVector pos = Location(u2, v2);
                    second.ballIndex = mesh.Add(Ball::Anchor(pos));
                    second.springsNeededMask |= OppositeDir(dir);
                    second.springsAddedMask = 0;
                }
            }
        }

        void AddMissingSpring(uint8_t dir, HexGridElement& first, int u2, int v2)
        {
            uint8_t missingSprings = first.springsNeededMask & ~first.springsAddedMask;
            if (missingSprings & dir)
            {
                uint8_t opp = OppositeDir(dir);
                HexGridElement& second = map.at(u2, v2);
                assert(second.ballIndex >= 0);
                assert(second.springsNeededMask & opp);
                assert(!(second.springsAddedMask & opp));
                mesh.Add(Spring(first.ballIndex, second.ballIndex));
                first.springsAddedMask |= dir;
                second.springsAddedMask |= opp;
            }
        }
    };

    // Create a mesh of hexagons. This reduces the spring overhead to 3 springs per ball.
    // hexWide is the number of hexagons whose centers lie along the direction [u = +1, v = -2].
    // hexFar  is the number of hexagons whose centers lie along the direction [u = +1, v = +1].
    MeshAudioParameters CreateHex(PhysicsMesh& mesh)
    {
        const float mass = 1.0e-6;
        const int hexWide = 2;
        const int hexFar = 3;
        const float spacing = MESH_DEFAULT_REST_LENGTH;
        const float peakVoltage = 10.0f;

        mesh.Reset();

        HexBuilder builder(mesh, 10, spacing, mass);

        for (int w = 0; w < hexWide; ++w)
            for (int f = 0; f < hexFar; ++f)
                builder.AddHexagon(w, f);

        builder.Finalize();

        MeshAudioParameters mp;
        memset(&mp, 0, sizeof(mp));

        mp.leftInputBallIndex   = builder.BallIndex(-1,  0, -1,  0);
        mp.rightInputBallIndex  = builder.BallIndex(+2, +2, +1,  0);
        mp.leftOutputBallIndex  = builder.BallIndex( 0, +2, -1, +1);
        mp.rightOutputBallIndex = builder.BallIndex(+1,  0, +1, -1);
        mp.leftStimulus1  = (spacing / peakVoltage) * PhysicsVector( 0.0f,  0.0f, 1.0f, 0);
        mp.leftStimulus2  = (spacing / peakVoltage) * PhysicsVector(+0.7f, -0.7f, 0.0f, 0);
        mp.rightStimulus1 = (spacing / peakVoltage) * PhysicsVector( 0.0f,  0.0f, 1.0f, 0);
        mp.rightStimulus2 = (spacing / peakVoltage) * PhysicsVector(-0.7f, +0.7f, 0.0f, 0);

        const float pos_factor = 4.0e+4;
        mp.leftResponse1  = pos_factor * PhysicsVector(0,  0, +1,  0);
        mp.leftResponse2  = pos_factor * PhysicsVector(0, -1,  0,  0);
        mp.rightResponse1 = pos_factor * PhysicsVector(0,  0, +1,  0);
        mp.rightResponse2 = pos_factor * PhysicsVector(0, +1,  0,  0);

        assert(mesh.GetBallAt(mp.leftInputBallIndex).IsAnchor());
        assert(mesh.GetBallAt(mp.rightInputBallIndex).IsAnchor());
        assert(mesh.GetBallAt(mp.leftOutputBallIndex).IsMobile());
        assert(mesh.GetBallAt(mp.rightOutputBallIndex).IsMobile());

        return mp;
    }
}
