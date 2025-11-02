#!/usr/bin/env python3
import sys

nMobileParticles = 42
nParticles = nMobileParticles + 2


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
    s = r'''//*****************************************
//****
//**** GENERATED CODE **** DO NOT EDIT
//****
//**** If you want to change this code,
//**** edit util/rk4_mesh_compiler.py,
//**** then run that script again.
//****
//*****************************************
#pragma once
namespace Sapphire
{
    namespace Vina
    {
        constexpr unsigned nMobileParticles = $N_MOBILE_PARTICLES$;
        constexpr unsigned nParticles = 2 + nMobileParticles;    // one anchor at each end of the chain

        struct VinaDeriv
        {
            float k = 1000 * defaultStiffness;            // stiffness/mass, where mass = 0.001 kg
            float restLength = 0.004;

            explicit VinaDeriv() {}

            void operator() (vina_state_t& y, const vina_state_t& x)
            {
                float acc;

'''.replace('$N_MOBILE_PARTICLES$', str(nMobileParticles))

    i = 0
    while i < nParticles:
        s += Line('y[{0:d}].pos = x[{0:d}].vel;'.format(i))
        i += 1

    s += '\n'
    i = 0
    while i < nParticles:
        if i > 0:
            s += '\n'
        needVel = True
        if i > 0:
            s += Line('acc = k*((x[{0:d}].pos - x[{1:d}].pos) - restLength);'.format(i, i-1))
            if i > 1:
                s += Line('y[{:d}].vel += acc;'.format(i-1))
            if i < nParticles-1:
                s += Line('y[{:d}].vel = -acc;'.format(i))
                needVel = False
        if needVel:
            s += Line('y[{:d}].vel = 0;'.format(i))
        i += 1

    s += r'''            }
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
