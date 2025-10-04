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
    s += Line('        particle[i].pos = PhysicsVector{horSpace*c, verSpace*r, 0, 0};')
    s += Line('        particle[i].vel = PhysicsVector{0, 0, 0, 0};')
    s += Line('    }')
    s += Line('}')
    return s


def GenPluck() -> str:
    s = ''
    s += Line('// For now, apply a velocity thump to a single ball.')
    s += Line('constexpr float thump = 1.0e-3;')
    s += Line('particle[1].vel[1] += thump;')
    return s


def GenUpdate() -> str:
    s = ''
    s += Line('// FIXFIXFIX')
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
#include "sapphire_simd.hpp"
namespace Sapphire
{
    namespace Vina
    {
        constexpr unsigned nRows = 2;
        constexpr unsigned nMobileColumns = $N_MOBILE_COLUMNS$;
        constexpr unsigned nColumns = $N_COLUMNS$;
        constexpr unsigned nParticles = nColumns * nRows;
        constexpr unsigned nMobileParticles = nMobileColumns * nRows;

        constexpr float horSpace = 0.01;    // horizontal spacing in meters
        constexpr float verSpace = 0.01;    // vertical spacing in meters

        inline unsigned particleIndex(unsigned c, unsigned r)
        {
            return r + c*nRows;
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
        };

        class VinaEngine
        {
        public:
            explicit VinaEngine()
            {
            }

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
                float left = 0;
                float right = 0;
$UPDATE$
                return VinaStereoFrame{left, right};
            }

        private:
            VinaParticle particle[nParticles];
        };
    }
}
'''
    s = s.replace('$N_MOBILE_COLUMNS$', str(nMobileColumns))
    s = s.replace('$N_COLUMNS$', str(nColumns))
    s = s.replace('$INITIALIZE$', GenInitialize())
    s = s.replace('$PLUCK$', GenPluck())
    s = s.replace('$UPDATE$', GenUpdate())
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
