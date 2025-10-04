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

        inline unsigned particleIndex(unsigned c, unsigned r)
        {
            assert(c < nColumns);
            assert(r < nRows);
            unsigned i = r + c*nRows;
            assert(i < nParticles);
            return i;
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


        class VinaDeriv
        {
        private:
            const spring_list_t& springs;

        public:
            PhysicsVector gravity;
            double stiffness = 30.0;
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
                    slope[i].vel = (i < nMobileParticles) ? gravity : PhysicsVector{};
                }

                for (const VinaSpring& s : springs)
                {
                    const VinaParticle& a = state.at(s.ia);
                    const VinaParticle& b = state.at(s.ib);
                    const PhysicsVector dr = b.pos - a.pos;
                    const double length = dr.mag();
                    const double fmag = stiffness*(length - restLength);
                    const PhysicsVector acc = (fmag/(mass * length)) * dr;
                    if (s.ia < nMobileParticles)  slope[s.ia].vel += acc;
                    if (s.ib < nMobileParticles)  slope[s.ib].vel -= acc;
                }
            }
        };

        using vina_sim_t = RungeKutta::ListSimulator<float, VinaParticle, VinaDeriv>;

        class VinaEngine
        {
        public:
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
                constexpr float thump = 1.0e-3;
                sim.state[1].vel[1] += thump;

            }

            VinaStereoFrame update(float sampleRateHz)
            {
                sim.step(1/sampleRateHz);
                return VinaStereoFrame{sim.state[5].vel[1], sim.state[7].vel[1]};

            }

        private:
            vina_sim_t sim;
                spring_list_t springs
                {
                };

        };
    }
}
