#!/usr/bin/env python3
import sys

def UpdateFileIfChanged(filename:str, newText:str) -> bool:
    # Do not write to the file unless the newText is different from
    # what already exists in the file, or we can't even read text from the file.
    # This allows us to change the modification date on a file only
    # when something has really changed. This way, we don't trick
    # `make` into doing unnecessary work
    # (like compiling C++ code that hasn't changed).
    try:
        with open(filename, 'rt') as infile:
            oldText = infile.read()
    except:
        oldText = ''
    if newText == oldText:
        print('Kept: ', filename)
        return False
    with open(filename, 'wt') as outfile:
        outfile.write(newText)
    print('Wrote:', filename)
    return True


def Line(s:str, indent:int = 4) -> str:
    return (' '*(4*indent)) + s + '\n'


def GenInitialize() -> str:
    s = ''
    s += Line('for (unsigned r = 0; r < nRows; ++r)')
    s += Line('{')
    s += Line('    for (unsigned c = 0; c < nColumns; ++c)')
    s += Line('    {')
    s += Line('        unsigned i = particleIndex(c, r);')
    s += Line('        sim.state[i].pos = PhysicsVector{horSpace*c, verSpace*r, 0, 0};')
    s += Line('        sim.state[i].vel = PhysicsVector{0, 0, 0, 0};')
    s += Line('    }')
    s += Line('}')
    return s


def GenPluck() -> str:
    s = ''
    s += Line('// For now, apply a velocity thump to a single ball.')
    s += Line('constexpr float thump = 1.0;')
    s += Line('sim.state[1].vel[1] += thump;')
    return s


def GenUpdate() -> str:
    speedMultiplier = 13.0      # FIXFIXFIX - tune the fundamental to C
    s = ''
    s += Line('sim.step({:g}/sampleRateHz);'.format(speedMultiplier))
    s += Line('brake(sampleRateHz, 0.25);')
    s += Line('return VinaStereoFrame{sim.state[5].vel[1], sim.state[7].vel[1]};')
    return s


def particleIndex(c:int, r:int) -> int:
    return r + c*2


def GenSprings(ncolumns:int) -> str:
    def spring(ia:int, ib:int) -> str:
        return Line('    VinaSpring{' + str(ia) + ', ' + str(ib) + '},')

    s = ''
    s += Line('spring_list_t springs')
    s += Line('{')
    for column in range(1, ncolumns):
        # lower string
        ia = particleIndex(column-1, 0)
        ib = particleIndex(column, 0)
        s += spring(ia, ib)
        # upper string
        ic = particleIndex(column-1, 1)
        id = particleIndex(column, 1)
        s += spring(ic, id)
        # weak force
        s += spring(ib, id)
    s += Line('};')
    return s


def GenVinaSourceCode() -> str:
    nMobileColumns = 13
    nColumns = nMobileColumns + 2
    s = r'''/***************************************************
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
        constexpr unsigned nMobileColumns = $N_MOBILE_COLUMNS$;
        constexpr unsigned nColumns = $N_COLUMNS$;
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


        class VinaDeriv
        {
        private:
            const spring_list_t& springs;

        public:
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

        class VinaEngine
        {
        public:
            explicit VinaEngine()
                : sim(VinaDeriv(springs), nParticles)
                {}

            void initialize()
            {
$INITIALIZE$
            }

            void pluck()
            {
$PLUCK$
            }

            VinaStereoFrame update(float sampleRateHz)
            {
$UPDATE$
            }

        private:
            vina_sim_t sim;
$SPRINGS$

            void brake(float sampleRateHz, float halfLifeSeconds)
            {
                const float factor = std::pow(0.5, 1/(sampleRateHz*halfLifeSeconds));
                for (unsigned i = 0; i < nParticles; ++i)
                    sim.state[i].vel *= factor;
            }
        };
    }
}
'''
    s = s.replace('$N_MOBILE_COLUMNS$', str(nMobileColumns))
    s = s.replace('$N_COLUMNS$', str(nColumns))
    s = s.replace('$INITIALIZE$', GenInitialize())
    s = s.replace('$PLUCK$', GenPluck())
    s = s.replace('$UPDATE$', GenUpdate())
    s = s.replace('$SPRINGS$', GenSprings(nColumns))
    return s


def main() -> int:
    if len(sys.argv) != 2:
        print('USAGE: rk4_mesh_compiler outfile.hpp')
        return 1
    outMeshHeaderFileName = sys.argv[1]
    vinaSourceCode = GenVinaSourceCode()
    UpdateFileIfChanged(outMeshHeaderFileName, vinaSourceCode)
    return 0


if __name__ == '__main__':
    sys.exit(main())
