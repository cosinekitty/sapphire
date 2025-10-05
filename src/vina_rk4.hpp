/***************************************************
***                                              ***
***     GENERATED CODE - !!! DO NOT EDIT !!!     ***
***                                              ***
***     To make changes to this code, edit:      ***
***     ../util/rk4/rk4_mesh_compiler.py         ***
***                                              ***
****************************************************/
#pragma once
#include <cassert>
#include "sapphire_simd.hpp"
#include "rk4_simulator.hpp"
namespace Sapphire
{
    namespace Vina
    {
        constexpr unsigned nRows = 2;
        constexpr unsigned nMobileColumns = 13;
        constexpr unsigned nColumns = 15;
        constexpr unsigned nParticles = nColumns * nRows;
        constexpr unsigned nMobileParticles = nMobileColumns * nRows;
        static_assert(nParticles > nMobileParticles);
        constexpr unsigned nAnchorParticles = nParticles - nMobileParticles;

        constexpr float horSpace = 0.01;    // horizontal spacing in meters
        constexpr float verSpace = 0.01;    // vertical spacing in meters

        constexpr unsigned particleIndex(unsigned c, unsigned r)
        {
            assert(c < nColumns);
            assert(r < nRows);
            unsigned i = r + c*nRows;
            assert(i < nParticles);
            return i;
        }

        constexpr bool isMobileIndex(unsigned i)
        {
            unsigned c = i/nRows;
            return (c>0) && (c<=nMobileColumns);
        }

        struct VinaStereoFrame
        {
            float sample[2];

            explicit VinaStereoFrame(float left, float right)
                : sample{left, right}
                {}
        };

        struct VinaParticle
        {
            PhysicsVector pos;
            PhysicsVector vel;

            explicit VinaParticle()
                {}

            explicit VinaParticle(PhysicsVector _pos, PhysicsVector _vel)
                : pos(_pos)
                , vel(_vel)
                {}

            friend VinaParticle operator * (float k, const VinaParticle& p)
            {
                return VinaParticle(k*p.pos, k*p.vel);
            }

            friend VinaParticle operator + (const VinaParticle& a, const VinaParticle& b)
            {
                return VinaParticle(a.pos + b.pos, a.vel + b.vel);
            }
        };

        using vina_state_t = std::vector<VinaParticle>;

        struct VinaSpring  // NOTE: eventually this class will be deleted, once the optimizer is working
        {
            const unsigned ia;
            const unsigned ib;

            explicit VinaSpring()
                : ia(-1)
                , ib(-1)
                {}

            explicit VinaSpring(unsigned aIndex, unsigned bIndex)
                : ia(aIndex)
                , ib(bIndex)
                {}
        };


        using spring_list_t = std::vector<VinaSpring>;


        struct VinaDeriv
        {
            const spring_list_t& springs;
            PhysicsVector gravity;
            double stiffness = 40.0;
            double restLength = 0.001;
            double mass = 0.001;

            explicit VinaDeriv(const spring_list_t& _springs)
                : springs(_springs)
                {}

            void operator() (vina_state_t& slope, const vina_state_t& state)
            {
                assert(nParticles == slope.size());
                assert(nParticles == state.size());

                for (unsigned i = 0; i < nParticles; ++i)
                {
                    slope[i].pos = state[i].vel;
                    slope[i].vel = isMobileIndex(i) ? gravity : PhysicsVector{};
                }

                for (const VinaSpring& s : springs)
                {
                    const VinaParticle& a = state.at(s.ia);
                    const VinaParticle& b = state.at(s.ib);
                    const PhysicsVector dr = b.pos - a.pos;
                    const double length = dr.mag();
                    const double fmag = stiffness*(length - restLength);
                    const PhysicsVector acc = (fmag/(mass * length)) * dr;
                    if (isMobileIndex(s.ia))  slope[s.ia].vel += acc;
                    if (isMobileIndex(s.ib))  slope[s.ib].vel -= acc;
                }
            }
        };

        using vina_sim_t = RungeKutta::ListSimulator<float, VinaParticle, VinaDeriv>;

        struct VinaEngine
        {
            float halfLifeSeconds = 0.2;

            explicit VinaEngine()
                : sim(VinaDeriv(springs), nParticles)
                {}

            void initialize()
            {
                for (unsigned r = 0; r < nRows; ++r)
                {
                    for (unsigned c = 0; c < nColumns; ++c)
                    {
                        unsigned i = particleIndex(c, r);
                        sim.state[i].pos = PhysicsVector{horSpace*c, verSpace*r, 0, 0};
                        sim.state[i].vel = PhysicsVector{0, 0, 0, 0};
                    }
                }
            }

            void pluck()
            {
                // For now, apply a velocity thump to a single ball.
                constexpr float thump = 1.0;
                sim.state[1].vel[1] += thump;
            }

            VinaStereoFrame update(float sampleRateHz)
            {
                sim.step(13/sampleRateHz);
                brake(sampleRateHz, halfLifeSeconds);
                return VinaStereoFrame{sim.state[5].vel[1], sim.state[7].vel[1]};
            }

            float maxSpeed() const
            {
                float s = 0;
                for (const VinaParticle& p : sim.state)
                    s = std::max(s, Quadrature(p.vel));
                return std::sqrt(s);
            }

            vina_sim_t sim;
                spring_list_t springs
                {
                    VinaSpring{0, 2},
                    VinaSpring{1, 3},
                    VinaSpring{2, 3},
                    VinaSpring{2, 4},
                    VinaSpring{3, 5},
                    VinaSpring{4, 5},
                    VinaSpring{4, 6},
                    VinaSpring{5, 7},
                    VinaSpring{6, 7},
                    VinaSpring{6, 8},
                    VinaSpring{7, 9},
                    VinaSpring{8, 9},
                    VinaSpring{8, 10},
                    VinaSpring{9, 11},
                    VinaSpring{10, 11},
                    VinaSpring{10, 12},
                    VinaSpring{11, 13},
                    VinaSpring{12, 13},
                    VinaSpring{12, 14},
                    VinaSpring{13, 15},
                    VinaSpring{14, 15},
                    VinaSpring{14, 16},
                    VinaSpring{15, 17},
                    VinaSpring{16, 17},
                    VinaSpring{16, 18},
                    VinaSpring{17, 19},
                    VinaSpring{18, 19},
                    VinaSpring{18, 20},
                    VinaSpring{19, 21},
                    VinaSpring{20, 21},
                    VinaSpring{20, 22},
                    VinaSpring{21, 23},
                    VinaSpring{22, 23},
                    VinaSpring{22, 24},
                    VinaSpring{23, 25},
                    VinaSpring{24, 25},
                    VinaSpring{24, 26},
                    VinaSpring{25, 27},
                    VinaSpring{26, 27},
                    VinaSpring{26, 28},
                    VinaSpring{27, 29},
                    VinaSpring{28, 29},
                };

            void brake(float sampleRateHz, float halfLifeSeconds)
            {
                const float factor = std::pow(0.5, 1/(sampleRateHz*halfLifeSeconds));
                for (VinaParticle& p : sim.state)
                    p.vel *= factor;
            }

            void settle(
                float sampleRateHz = 48000,
                float halfLifeSeconds = 0.1,
                float settleTimeSeconds = 5.0)
            {
                const unsigned nFrames = static_cast<unsigned>(sampleRateHz * settleTimeSeconds);
                for (unsigned frameCount = 0; frameCount < nFrames; ++frameCount)
                {
                    update(sampleRateHz);
                    brake(sampleRateHz, halfLifeSeconds);
                }
            }
        };
    }
}
