/*
    meshgen.cpp  -  Don Cross <cosinekitty@gmail.com>

    https://github.com/cosinekitty/sapphire
*/
#include <cstdio>
#include "mesh_hex.hpp"

static int WritePrefix(FILE *outfile);
static int GenAudioParameters(FILE *outfile, const Sapphire::MeshAudioParameters& mp);
static int GenConstructor(FILE *outfile, const Sapphire::PhysicsMesh& mesh);
static int GenDampenFunction(FILE *outfile, const Sapphire::PhysicsMesh& mesh, int nmobile);
static int GenForceFunction(FILE *outfile, const Sapphire::PhysicsMesh& mesh);
static int GenExtrapolateFunction(FILE *outfile, const Sapphire::PhysicsMesh& mesh, int nmobile);
static int WriteSuffix(FILE *outfile);

static int GenerateMeshCode(
    const char *outFileName,
    const Sapphire::PhysicsMesh& mesh,
    const Sapphire::MeshAudioParameters& mp)
{
    if (0 == remove(outFileName))
        printf("Deleted: %s\n", outFileName);

    const int nballs = mesh.NumBalls();
    int nmobile = 0;
    while (nmobile < nballs && mesh.GetBallAt(nmobile).IsMobile())
        ++nmobile;

    printf("GenerateMeshCode: balls=%d, mobile=%d\n", nballs, nmobile);
    for (int i = nmobile; i < nballs; ++i)
    {
        if (mesh.GetBallAt(i).IsMobile())
        {
            printf("GenerateMeshCode(FATAL): found mobile ball at index %d\n", i);
            return 1;
        }
    }

    FILE *outfile = fopen(outFileName, "wt");
    if (outfile == nullptr)
    {
        printf("GenerateMeshCode: Could not open output file: %s\n", outFileName);
        return 1;
    }

    int rc =
        WritePrefix(outfile) ||
        GenConstructor(outfile, mesh) ||
        GenDampenFunction(outfile, mesh, nmobile) ||
        GenForceFunction(outfile, mesh) ||
        GenExtrapolateFunction(outfile, mesh, nmobile) ||
        GenAudioParameters(outfile, mp) ||
        WriteSuffix(outfile)
    ;

    fclose(outfile);

    if (rc == 0)
        printf("Wrote: %s\n", outFileName);
    else
        remove(outFileName);

    return rc;
}


int main()
{
    using namespace Sapphire;
    PhysicsMesh mesh;
    MeshAudioParameters mp = CreateHex(mesh);
    return GenerateMeshCode("../src/elastika_mesh.cpp", mesh, mp);
}


static int WritePrefix(FILE *outfile)
{
    fprintf(outfile, "//****  GENERATED CODE  ****  DO NOT EDIT  *****\n\n");
    fprintf(outfile, "#include \"elastika_mesh.hpp\"\n\n");
    fprintf(outfile, "namespace Sapphire\n");
    fprintf(outfile, "{\n");
    return 0;
}


static int WriteSuffix(FILE *outfile)
{
    fprintf(outfile, "}\n");
    return 0;
}


static int GenAudioParmVector(FILE *outfile, const char *name, const Sapphire::PhysicsVector& v)
{
    fprintf(outfile, "        mp.%s = PhysicsVector{%0.6g, %0.6g, %0.6g, 0};\n", name, v[0], v[1], v[2]);
    return 0;
}


static int GenAudioParameters(FILE *outfile, const Sapphire::MeshAudioParameters& mp)
{
    fprintf(outfile, "    MeshAudioParameters ElastikaMesh::getAudioParameters()\n");
    fprintf(outfile, "    {\n");
    fprintf(outfile, "        MeshAudioParameters mp;\n");
    fprintf(outfile, "        mp.leftInputBallIndex = %d;\n", mp.leftInputBallIndex);
    fprintf(outfile, "        mp.rightInputBallIndex = %d;\n", mp.rightInputBallIndex);
    fprintf(outfile, "        mp.leftOutputBallIndex = %d;\n", mp.leftOutputBallIndex);
    fprintf(outfile, "        mp.rightOutputBallIndex = %d;\n", mp.rightOutputBallIndex);
    fprintf(outfile, "        mp.leftVarMassBallIndex = %d;\n", mp.leftVarMassBallIndex);
    fprintf(outfile, "        mp.rightVarMassBallIndex = %d;\n", mp.rightVarMassBallIndex);
    if (GenAudioParmVector(outfile, "leftInputDir1",   mp.leftInputDir1))   return 1;
    if (GenAudioParmVector(outfile, "leftInputDir2",   mp.leftInputDir2))   return 1;
    if (GenAudioParmVector(outfile, "rightInputDir1",  mp.rightInputDir1))  return 1;
    if (GenAudioParmVector(outfile, "rightInputDir2",  mp.rightInputDir2))  return 1;
    if (GenAudioParmVector(outfile, "leftOutputDir1",  mp.leftOutputDir1))  return 1;
    if (GenAudioParmVector(outfile, "leftOutputDir2",  mp.leftOutputDir2))  return 1;
    if (GenAudioParmVector(outfile, "rightOutputDir1", mp.rightOutputDir1)) return 1;
    if (GenAudioParmVector(outfile, "rightOutputDir2", mp.rightOutputDir2)) return 1;
    fprintf(outfile, "        return mp;\n");
    fprintf(outfile, "    }\n");
    return 0;
}


static int GenConstructor(FILE *outfile, const Sapphire::PhysicsMesh& mesh)
{
    using namespace Sapphire;

    fprintf(outfile, "    ElastikaMesh::ElastikaMesh()\n");
    fprintf(outfile, "    {\n");

    // Balls
    const int nballs = mesh.NumBalls();
    for (int i = 0; i < nballs; ++i)
    {
        const Ball& b = mesh.GetBallAt(i);
        fprintf(outfile, "        Add(Ball(%0.7g, %0.7g, %0.7g, %0.7g));\n", b.mass, b.pos[0], b.pos[1], b.pos[2]);
    }

    fprintf(outfile, "\n");

    // Springs
    const SpringList& slist = mesh.GetSprings();
    for (const Spring& s : slist)
    {
        fprintf(outfile, "        Add(Spring(%d, %d));\n", s.ballIndex1, s.ballIndex2);
    }

    fprintf(outfile, "    }\n");
    fprintf(outfile, "\n");
    return 0;
}


static void EmitCrossProduct(
    FILE *outfile,
    const Sapphire::PhysicsMesh& mesh,
    std::vector<int>& emitted,
    int index)
{
    if (mesh.GetBallAt(index).IsMobile() && !emitted.at(index))
    {
        emitted.at(index) = 1;
        fprintf(outfile, "        forceList[%d] = Cross(blist[%d].vel, magnet);\n", index, index);
    }
}


static int GenForceFunction(FILE *outfile, const Sapphire::PhysicsMesh& mesh)
{
    using namespace Sapphire;

    const int nballs = mesh.NumBalls();
    std::vector<int> emittedCrossProduct;
    emittedCrossProduct.resize(nballs, 0);

    fprintf(outfile, "    void ElastikaMesh::CalcForces(const BallList& blist, PhysicsVectorList& forceList)\n");
    fprintf(outfile, "    {\n");

    const char *updateFormula = "((stiffness * (dist - restLength)) / dist) * dr";
    const SpringList& slist = mesh.GetSprings();
    bool firstUpdate = true;
    for (const Spring& s : slist)
    {
        if (!firstUpdate)
            fprintf(outfile, "\n");

        EmitCrossProduct(outfile, mesh, emittedCrossProduct, s.ballIndex1);
        EmitCrossProduct(outfile, mesh, emittedCrossProduct, s.ballIndex2);
        const Ball& b1 = mesh.GetBallAt(s.ballIndex1);
        const Ball& b2 = mesh.GetBallAt(s.ballIndex2);
        fprintf(outfile, "        ");
        if (firstUpdate)
            fprintf(outfile, "PhysicsVector ");
        fprintf(outfile, "dr = blist[%d].pos - blist[%d].pos;\n", s.ballIndex2, s.ballIndex1);
        fprintf(outfile, "        ");
        if (firstUpdate)
            fprintf(outfile, "float ");
        fprintf(outfile, "dist = Magnitude(dr);\n");
        fprintf(outfile, "        if (dist >= 1.0e-9f)\n");
        if (b1.IsMobile() && b2.IsAnchor())
        {
            fprintf(outfile, "            forceList[%d] += %s;\n", s.ballIndex1, updateFormula);
        }
        else if (b2.IsMobile() && b1.IsAnchor())
        {
            fprintf(outfile, "            forceList[%d] -= %s;\n", s.ballIndex2, updateFormula);
        }
        else if (b1.IsMobile() && b2.IsMobile())
        {
            fprintf(outfile, "        {\n");
            fprintf(outfile, "            PhysicsVector force = %s;\n", updateFormula);
            fprintf(outfile, "            forceList[%2d] += force;\n", s.ballIndex1);
            fprintf(outfile, "            forceList[%2d] -= force;\n", s.ballIndex2);
            fprintf(outfile, "        }\n");
        }
        else
        {
            printf("GenForceFunction(FATAL): both balls are anchors: %d, %d\n", s.ballIndex1, s.ballIndex2);
            return 1;
        }
        firstUpdate = false;
    }

    fprintf(outfile, "    }\n");
    fprintf(outfile, "\n");

    int rc = 0;
    for (int i = 0; i < nballs; ++i)
    {
        if (mesh.GetBallAt(i).IsMobile() && !emittedCrossProduct.at(i))
        {
            printf("GenForceFunction: Failed to emit cross product for ball %d.\n", i);
            rc = 1;
        }
    }

    return rc;
}


static int GenDampenFunction(FILE *outfile, const Sapphire::PhysicsMesh& mesh, int nmobile)
{
    // Unroll the dampen loop.
    fprintf(outfile, "    void ElastikaMesh::Dampen(BallList& blist, float dt, float halflife)\n");
    fprintf(outfile, "    {\n");
    fprintf(outfile, "        const float damp = std::pow(0.5f, dt/halflife);\n");
    for (int i = 0; i < nmobile; ++i)
        fprintf(outfile, "        blist[%2d].vel *= damp;\n", i);
    fprintf(outfile, "    }\n\n");

    return 0;
}


static int GenExtrapolateFunction(FILE *outfile, const Sapphire::PhysicsMesh& mesh, int nmobile)
{
    fprintf(outfile, "    void ElastikaMesh::Extrapolate(float dt)\n");
    fprintf(outfile, "    {\n");
    fprintf(outfile, "        const float speedLimitSquared = speedLimit * speedLimit;\n");
    fprintf(outfile, "        const Ball* curr = currBallList.data();\n");
    fprintf(outfile, "        Ball* next = nextBallList.data();\n");
    fprintf(outfile, "        float speedSquared;\n");
    fprintf(outfile, "\n");

    for (int i = 0; i < nmobile; ++i)
    {
        fprintf(outfile, "        next->vel = curr->vel + ((dt / curr->mass) * forceList[%d]);\n", i);
        fprintf(outfile, "        speedSquared = Quadrature(next->vel);\n");
        fprintf(outfile, "        if (speedSquared > speedLimitSquared)\n");
        fprintf(outfile, "            next->vel *= speedLimit / std::sqrt(speedSquared);\n");
        fprintf(outfile, "        next->pos = curr->pos + ((dt/2) * (curr->vel + next->vel));\n");
        if (i+1 < nmobile)
        {
            fprintf(outfile, "        ++curr;\n");
            fprintf(outfile, "        ++next;\n");
            fprintf(outfile, "\n");
        }
    }

    fprintf(outfile, "    }\n\n");
    return 0;
}
