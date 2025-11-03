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
#include <array>

namespace Sapphire
{
    namespace Vina
    {
        constexpr unsigned nMobileParticles = $N_MOBILE_PARTICLES$;
        constexpr unsigned nParticles = 2 + nMobileParticles;    // one anchor at each end of the chain
        constexpr unsigned pluckBaseLeft    = 1 + (1*nMobileParticles)/5;
        constexpr unsigned pluckBaseRight   = 1 + (2*nMobileParticles)/5;
        constexpr unsigned leftOutputIndex  = 1 + (3*nMobileParticles)/5;
        constexpr unsigned rightOutputIndex = 1 + (4*nMobileParticles)/5;
        constexpr float horSpace = 0.01;    // horizontal spacing between adjacent particles in meters
        constexpr float restLength = 0.004;
        constexpr float silenceTension = 0.534;    // [N] desired tension force in each spring when silent
        constexpr float defaultStiffness = silenceTension / (horSpace - restLength);
        constexpr float tuning = 75.54;

        using vina_state_t = std::array<VinaParticle, nParticles>;

        struct VinaDeriv
        {
            float k = 1000 * defaultStiffness;            // stiffness/mass, where mass = 0.001 kg

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


        struct VinaAdd
        {
            void operator() (vina_state_t& t, const vina_state_t& a, const vina_state_t& b)
            {
                for (std::size_t i=0; i < nParticles; ++i)
                    t[i] = a[i] + b[i];
            }
        };


        struct VinaMul
        {
            void operator() (vina_state_t& t, const vina_state_t& a, float k)
            {
                for (std::size_t i=0; i < nParticles; ++i)
                    t[i] = k * a[i];
            }
        };


        using sim_base_t = RungeKutta::Simulator<float, vina_state_t, VinaDeriv, VinaAdd, VinaMul>;

        class VinaSimulator : public sim_base_t
        {
        public:
            explicit VinaSimulator()
                : sim_base_t(VinaDeriv(), VinaAdd(), VinaMul())
                {}
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
