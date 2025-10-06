#!/usr/bin/env python3
import sys

nMobileColumns = 42
nColumns = nMobileColumns + 2


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


def GenVinaSourceCode() -> str:
    s = r'''//**** GENERATED CODE **** DO NOT EDIT ****
#pragma once
namespace Sapphire
{
    namespace Vina
    {
        struct VinaDeriv
        {
            PhysicsVector gravity;
            double stiffness = 89.0;
            double restLength = 0.004;
            double mass = 1.0e-03;

            explicit VinaDeriv() {}

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
    }
}
'''
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
