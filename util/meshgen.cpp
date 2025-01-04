/*
    meshgen.cpp  -  Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/
#include <cstdio>
#include "elastika_engine.hpp"

static int GenerateMeshCode(const char *outFileName, Sapphire::PhysicsMesh& mesh);

int main()
{
    Sapphire::PhysicsMesh mesh;
    CreateHex(mesh);
    return GenerateMeshCode("../src/elastika_mesh.cpp", mesh);
}


static int GenerateMeshCode(const char *outFileName, Sapphire::PhysicsMesh& mesh)
{
    FILE *outfile = fopen(outFileName, "wt");
    if (outfile == nullptr)
    {
        printf("GenerateMeshCode: Could not open output file: %s\n", outFileName);
        return 1;
    }

    fprintf(outfile, "//****  GENERATED CODE  ****  DO NOT EDIT  *****\n\n");
    fprintf(outfile, "namespace Sapphire\n");
    fprintf(outfile, "{\n");
    fprintf(outfile, "}\n");

    fclose(outfile);
    printf("Wrote: %s\n", outFileName);
    return 0;
}

