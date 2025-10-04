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
    s = r'''//*** GENERATED CODE - !!! DO NOT EDIT !!! ***
#pragma once
namespace Sapphire
{
    namespace Vina
    {
    }
}
'''
    return s


def main() -> int:
    if len(sys.argv) != 3:
        print('USAGE: rk4_mesh_compiler in_mesh.json out_mesh.hpp')
        return 1
    inMeshJsonFileName = sys.argv[1]
    outMeshHeaderFileName = sys.argv[2]
    print('rk4_mesh_compiler: translating {} ==> {}'.format(inMeshJsonFileName, outMeshHeaderFileName))
    vinaSourceCode = GenVinaSourceCode()
    UpdateFileIfChanged(outMeshHeaderFileName, vinaSourceCode)
    return 0

if __name__ == '__main__':
    sys.exit(main())
