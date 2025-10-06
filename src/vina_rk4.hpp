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
        constexpr unsigned nRows = 1;
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
        extern const vina_state_t EngineInit;

        struct VinaDeriv
        {
            PhysicsVector gravity;
            double stiffness = 89.0;
            double restLength = 0.004;
            double mass = 1.0e-03;

            explicit VinaDeriv()
                {}

            void operator() (vina_state_t& slope, const vina_state_t& state)
            {
                assert(nParticles == slope.size());
                assert(nParticles == state.size());

                slope[0].pos = state[0].vel;
                slope[0].vel = PhysicsVector{};
                for (unsigned i = 1; i < nParticles; ++i)
                {
                    slope[i].pos = state[i].vel;
                    slope[i].vel = isMobileIndex(i) ? gravity : PhysicsVector{};
                    const VinaParticle& a = state.at(i-1);
                    const VinaParticle& b = state.at(i-0);
                    const PhysicsVector dr = b.pos - a.pos;
                    const double length = dr.mag();
                    const double fmag = stiffness*(length - restLength);
                    const PhysicsVector acc = (fmag/(mass * length)) * dr;
                    if (isMobileIndex(i-1))  slope[i-1].vel += acc;
                    if (isMobileIndex(i-0))  slope[i-0].vel -= acc;
                }
            }
        };

        using vina_sim_t = RungeKutta::ListSimulator<float, VinaParticle, VinaDeriv>;

        struct VinaEngine
        {
            float halfLifeSeconds = 0.2;
            unsigned oversample = 1;

            explicit VinaEngine()
                : sim(VinaDeriv(), nParticles)
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
                constexpr float thump = 0.1;
                sim.state[3].vel[0] += thump;
            }

            VinaStereoFrame update(float sampleRateHz)
            {
                const float dt = 13 / sampleRateHz;
                const float et = dt / oversample;
                for (unsigned k = 0; k < oversample; ++k)
                    sim.step(et);
                brake(sampleRateHz, halfLifeSeconds);
                return VinaStereoFrame{sim.state[9].vel[0], sim.state[10].vel[0]};
            }

            float maxSpeed() const
            {
                float s = 0;
                for (const VinaParticle& p : sim.state)
                    s = std::max(s, Quadrature(p.vel));
                return std::sqrt(s);
            }

            vina_sim_t sim;

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

            void setPreSettledState()
            {
                assert(sim.state.size() == EngineInit.size());
                sim.state = EngineInit;
            }
        };
    }
}
