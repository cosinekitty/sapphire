#include "mesh.hpp"

// Sapphire mesh physics engine, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    PhysicsVector VectorFromJson(json_t *parent, const char *key)
    {
        PhysicsVector vec;
        json_t *array = json_object_get(parent, key);
        if (json_array_size(array) == 3)
        {
            vec.x = json_number_value(json_array_get(array, 0));
            vec.y = json_number_value(json_array_get(array, 1));
            vec.z = json_number_value(json_array_get(array, 2));
        }
        return vec;
    }

    json_t *JsonFromVector(PhysicsVector vec)
    {
        json_t *array = json_array();
        json_array_append(array, json_real(vec.x));
        json_array_append(array, json_real(vec.y));
        json_array_append(array, json_real(vec.z));
        return array;
    }

    void MeshFromJson(PhysicsMesh &mesh, json_t* root)
    {
        mesh.Reset();

        PhysicsVector gravity = VectorFromJson(root, "gravity");
        mesh.SetGravity(gravity);

        json_t* stiffnessJson = json_object_get(root, "stiffness");
        if (json_is_number(stiffnessJson))
        {
            double stiffness = json_number_value(stiffnessJson);
            mesh.SetStiffness(stiffness);
        }

        json_t* restLengthJson = json_object_get(root, "restLength");
        if (json_is_number(restLengthJson))
        {
            double restLength = json_number_value(restLengthJson);
            mesh.SetRestLength(restLength);
        }

        json_t* speedLimitJson = json_object_get(root, "speedLimit");
        if (json_is_number(speedLimitJson))
        {
            double speedLimit = json_number_value(speedLimitJson);
            mesh.SetSpeedLimit(speedLimit);
        }

        json_t* ballsJson = json_object_get(root, "balls");
        if (json_is_array(ballsJson))
        {
            size_t nballs = json_array_size(ballsJson);
            for (size_t i = 0; i < nballs; ++i)
            {
                json_t *bJson = json_array_get(ballsJson, i);
                if (json_is_object(bJson))
                {
                    json_t *massJson = json_object_get(bJson, "mass");
                    double mass = json_number_value(massJson);
                    PhysicsVector pos = VectorFromJson(bJson, "pos");
                    PhysicsVector vel = VectorFromJson(bJson, "vel");
                    mesh.Add(Ball(mass, pos, vel));
                }
            }
        }

        json_t* springsJson = json_object_get(root, "springs");
        if (json_is_array(springsJson))
        {
            size_t nsprings = json_array_size(springsJson);
            for (size_t i = 0; i < nsprings; ++i)
            {
                json_t *sJson = json_array_get(springsJson, i);
                if (json_array_size(sJson) == 2)
                {
                    int ballIndex1 = json_integer_value(json_array_get(sJson, 0));
                    int ballIndex2 = json_integer_value(json_array_get(sJson, 1));
                    mesh.Add(Spring(ballIndex1, ballIndex2));
                }
            }
        }
    }

    json_t *JsonFromMesh(const PhysicsMesh &mesh)
    {
        json_t *root = json_object();

        json_object_set_new(root, "gravity", JsonFromVector(mesh.GetGravity()));
        json_object_set_new(root, "stiffness", json_real(mesh.GetStiffness()));
        json_object_set_new(root, "restLength", json_real(mesh.GetRestLength()));
        json_object_set_new(root, "speedLimit", json_real(mesh.GetSpeedLimit()));

        json_t *ballsJson = json_array();
        for (const Ball &b : mesh.GetBalls())
        {
            json_t *bJson = json_object();
            json_object_set_new(bJson, "mass", json_real(b.mass));
            json_object_set_new(bJson, "pos", JsonFromVector(b.pos));
            json_object_set_new(bJson, "vel", JsonFromVector(b.vel));
            json_array_append(ballsJson, bJson);
        }
        json_object_set_new(root, "balls", ballsJson);

        json_t *springsJson = json_array();
        for (const Spring& s : mesh.GetSprings())
        {
            json_t *array = json_array();
            json_array_append(array, json_integer(s.ballIndex1));
            json_array_append(array, json_integer(s.ballIndex2));
            json_array_append(springsJson, array);
        }
        json_object_set_new(root, "springs", springsJson);

        return root;
    }
}
