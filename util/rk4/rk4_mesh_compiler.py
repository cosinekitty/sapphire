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


def GenVinaSourceCode() -> str:
    nMobileColumns = 13
    s = r'''//*** GENERATED CODE - !!! DO NOT EDIT !!! ***
#pragma once
namespace Sapphire
{
    namespace Vina
    {
        constexpr unsigned nRows = 2;
        constexpr unsigned nMobileColumns = $nMobileColumns$;
        constexpr unsigned nColumns = nMobileColumns + 2;

        struct VinaStereoFrame
        {
            float sample[2];

            explicit VinaStereoFrame(float left, float right)
                : sample{left, right}
                {}
        };

        class VinaEngine
        {
        public:
            explicit VinaEngine()
            {
            }

            void initialize()
            {
            }

            void pluck()
            {
            }

            VinaStereoFrame update(float sampleRateHz)
            {
                float left = 0;
                float right = 0;
                return VinaStereoFrame{left, right};
            }

        private:
        };
    }
}
'''
    s = s.replace('$nMobileColumns$', str(nMobileColumns))
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
